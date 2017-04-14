#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "object_array.h"


ObjectArray::ObjectArray(size_t capacity) {
    this->objects = (Object*)malloc(capacity * sizeof(Object));
    if (!this->objects) {
        printf("unable to initialize objects in ObjectArray\n");
        return;
    }
    this->capacity = capacity;
    this->len = 0;
}

/**
 * dealloc an ObjectArray
 */
ObjectArray::~ObjectArray() {
    if (this->objects) {
       free(this->objects);
    }
}

/**
 * remove an object at a specified index i, replace the i-th object with the last obejct in the ObjectArray
 */
void 
ObjectArray::remove_object_at(size_t i) {
    if (i != this->len) {
        this->objects[i] = this->objects[this->len - 1];
    }
    this->len--;
}

/**
 * add object into the ObjectArray
 */
void 
ObjectArray::add(Object object) {
    if (this->len >= this->capacity) {
        size_t new_capacity = this->capacity * 2;
        Object* realloc_ptr = (Object*)realloc(this->objects, new_capacity * sizeof(Object));
        if (realloc_ptr == NULL) {
            fprintf(stderr, "Realloc error in ObjectArray.\n");
            return; //no erro handling here
        }
        this->objects = realloc_ptr;
        this->capacity = new_capacity;
    }
    this->objects[this->len] = object;
    this->len++;
}
