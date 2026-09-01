#ifndef CALLBACK_H
#define CALLBACK_H

// The type of a function that writes text.
//
// Each module that writes something takes a function of this type. printf has
// this type, thus a caller can give printf directly. A caller on a target with
// no console gives its own function, for example one that writes to a serial
// port. A caller that gives NULL gets printf.
typedef int (*print_t)(const char*, ...);

#endif//CALLBACK_H
