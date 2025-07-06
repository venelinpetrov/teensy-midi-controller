#include <Arduino.h>
#include "midi_map.h"
#include <sreg.h>

/* Program Parameters */
#define NUM_ANALOG_INPUTS_ 8
#define NUM_DIGITAL_INPUTS_ 21
#define CHANNEL 1
#define DEBOUNCE_TIME 20

/* Pins */
#define POWER_LED 11

/* 74HC4051 */
#define S0 4 /* select input 0*/
#define S1 9 /* select input 1*/
#define S2 10 /* select input 2*/
#define Z0 21 /* common output */
#define Z1 0 /* common output */

/* 74HC595 */
#define DS 2 /* serial data input*/
#define SHCP 1 /* clock */
#define STCP 8 /* latch */

/* 74HC165 */
#define Q7 20 /* serial data output */
#define PL 19 /* async parallel load input */

/* Variable declarations and initializations */
struct analog_input {
	uint8_t delta = 0;
	uint8_t prev = 0;
	uint8_t value = 0;
} analog_inputs[NUM_ANALOG_INPUTS_];

struct digital_input {
	uint8_t delta = 0;
	uint8_t prev = LOW;
	uint8_t state = HIGH;
	long timestamp = 0ul;
} digital_inputs[NUM_DIGITAL_INPUTS_];

uint8_t analog_input_increment;
uint8_t digital_input_increment;

/* Function declarations */
void on_control_change(uint8_t channel, uint8_t midi_cc, uint8_t value);
void on_note_on(uint8_t channel, uint8_t midi_cc, uint8_t velocity);
void on_note_off(uint8_t channel, uint8_t midi_cc, uint8_t velocity);
void send_control_change_digital(uint8_t pin, uint8_t midi_cc, uint8_t channel);
struct analog_input read_analog_input(uint8_t pin);
struct digital_input read_digital_input(uint8_t pin, uint8_t midi_cc, uint8_t channel);
void enable_mux_pin(uint8_t pin);
void send_control_change(uint8_t pin, uint8_t midi_cc, uint8_t channel);
void send_note_on_off(uint8_t pin, uint8_t midi_cc, uint8_t channel);

void setup()
{
	Serial.begin(31250);

	pinMode(S0, OUTPUT);
	pinMode(S1, OUTPUT);
	pinMode(S2, OUTPUT);
	pinMode(PL, OUTPUT);

	usbMIDI.setHandleControlChange(on_control_change);
	usbMIDI.setHandleNoteOn(on_note_on);
	usbMIDI.setHandleNoteOff(on_note_off);

	/*ds, shcp, stcp, init_value, sreg_count*/
	sreg_init(DS, SHCP, STCP, 0b00000000, 1);

	delay(100);
	digitalWrite(POWER_LED, HIGH);
}


void loop()
{
	analog_input_increment = 0;
	digital_input_increment = 0;
	sreg_latch_low();
	usbMIDI.read();
	enable_mux_pin(0);
	send_control_change(Z0, PAN_CC + 1, CHANNEL);

	enable_mux_pin(1);
	send_control_change(Z0, SEND_CC + 3, CHANNEL);

	enable_mux_pin(2);
	send_control_change(Z0, SEND_CC + 1, CHANNEL);
	send_control_change_digital(Z1, SEND_DOWN_CC, CHANNEL);

	enable_mux_pin(3);
	send_control_change(Z0, VOLUME_CC + 1, CHANNEL);

	enable_mux_pin(4);
	send_control_change(Z0, SEND_CC, CHANNEL);
	send_note_on_off(Z1, DEVICE_MODE_CC, CHANNEL);

	enable_mux_pin(5);
	send_control_change(Z0, VOLUME_CC, CHANNEL);
	send_control_change_digital(Z1, SEND_UP_CC, CHANNEL);

	enable_mux_pin(6);
	send_control_change(Z0, SEND_CC + 2, CHANNEL);
	send_control_change_digital(Z1, DEVICE_LEFT_CC, CHANNEL);

	enable_mux_pin(7);
	send_control_change(Z0, PAN_CC, CHANNEL);
	send_control_change_digital(Z1, DEVICE_RIGHT_CC, CHANNEL);

	sreg_latch_high();

	int val165 = 0;
 	int sreg_count_ = 2;

	digitalWrite(PL, LOW);
	digitalWrite(PL, HIGH);

	for (int i = (4 << sreg_count_) - 1; i >= 0; i--) {
		val165 = (val165 & ~(1 << i)) | (!!digitalRead(Q7) << i);
		sreg_write_bit(i, digitalRead(Q7));
		send_control_change_digital(Q7, pin_to_midi[i], CHANNEL);
		digitalWrite(SHCP, LOW);
		digitalWrite(SHCP, HIGH);
	}
	Serial.printf("\n%d", val165);

	delay(5);
}


/* Functions definitions*/
void on_control_change(uint8_t channel, uint8_t midi_cc, uint8_t value)
{
	// Serial.printf("channel: %d\t control: %d\t value: %d\n", channel, midi_cc, value);
}

void on_note_on(uint8_t channel, uint8_t midi_cc, uint8_t velocity)
{
	Serial.printf("NOTE_ON channel: %d\t control: %d\t value: %d\n", channel, midi_cc, velocity);
}

void on_note_off(uint8_t channel, uint8_t midi_cc, uint8_t velocity)
{
	Serial.printf("NOTE_OFF channel: %d\t control: %d\t value: %d\n", channel, midi_cc, velocity);
}

void enable_mux_pin(uint8_t pin)
{
	digitalWrite(S0, pin & 0x01);
	digitalWrite(S1, (pin >> 1) & 0x01);
	digitalWrite(S2, (pin >> 2) & 0x01);
}

struct analog_input read_analog_input(uint8_t pin)
{
	struct analog_input *input = &analog_inputs[analog_input_increment];

	input->value = analogRead(pin) >> 3;
	input->delta = input->prev - input->value;
	input->prev = input->value;
	analog_input_increment++;

	return *input;
}

struct digital_input read_digital_input(uint8_t pin)
{
	struct digital_input *input = &digital_inputs[digital_input_increment];

	input->state = digitalRead(pin);
	input->delta = input->prev ^ input->state;
	input->prev = input->state;
	digital_input_increment++;

	return *input;
}

void send_control_change(uint8_t pin, uint8_t midi_cc, uint8_t channel)
{
	struct analog_input input = read_analog_input(pin);

	if (input.delta != 0)
		usbMIDI.sendControlChange(midi_cc, input.value, channel);
}

void send_note_on_off(uint8_t pin, uint8_t midi_cc, uint8_t channel)
{
	struct digital_input input = read_digital_input(pin);

	if (input.delta && millis() - input.timestamp > DEBOUNCE_TIME) {
		if (input.state == HIGH) {
			usbMIDI.sendNoteOn(midi_cc, 127, channel);
			input.state = LOW;

		} else {
			input.state = HIGH;
			usbMIDI.sendNoteOff(midi_cc, 0, channel);
		}

		input.timestamp = millis();
	}
}

void send_control_change_digital(uint8_t pin, uint8_t midi_cc, uint8_t channel)
{
	struct digital_input input = read_digital_input(pin);

	if (input.delta && millis() - input.timestamp > DEBOUNCE_TIME) {
		if (input.state == HIGH) {
			input.state = LOW;
			usbMIDI.sendControlChange(midi_cc, 127, channel);

		} else {
			input.state = HIGH;
			usbMIDI.sendControlChange(midi_cc, 0, channel);
		}

		input.timestamp = millis();
	}
}