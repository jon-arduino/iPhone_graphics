This is the general WIP for the current rev.
Rev 2.1
    Add Keys to Graphics screen to run test1 and test2
    add commands to back channel to cause tests to run
    put ping in the RTOS as a background task so blocking main will not block ping....
    Pumpble is also moved to background (out of loop) to keep all this consistant and in the background
    Ping was in loop() — now moved to a background FreeRTOS task. The task runs every 100ms and owns all three time-sensitive operations: pump_BLE_txQ(), wifiManager.tick(), and ble.tick().
    loop() only handles transport selection, UI init, console polling, pending tests, and GPS rendering.



    