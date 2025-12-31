#ifndef KERNELCB_H
#define KERNELCB_H

void on_keyboard_pressed(uint8_t scancode, uint8_t final_char);
void on_mouse_update(int32_t x_rel, int32_t y_rel, uint8_t left_button, uint8_t middle_button, uint8_t right_button);

#endif // KERNELCB_H