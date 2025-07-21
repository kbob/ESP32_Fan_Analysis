extern void setup(), loop();

extern "C" void app_main() {
    setup();
    while (1) {
        loop();
    }
}