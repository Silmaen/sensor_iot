#ifndef NATIVE
#include <Arduino.h>
#endif

void setup() {
#ifndef NATIVE
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port
    }
    Serial.println("Thermo starting...");
#endif
}

void loop() {
#ifndef NATIVE
    delay(1000);
#endif
}

#ifdef NATIVE
int main() {
    setup();
    loop();
    return 0;
}
#endif
