

1. Start distrib server
<pre>
    make setup # In dev/server/distrib
    make run
</pre>
2. Put the distrib certificate in target project (dev/target/main/server_cert.pem)
<pre>
    make get-cert # In dev/server/distrib
    cp server_cert.pem ../../target/main/ 
</pre>

3. Build target project

<pre>
    make enter_idf # In dev/target
    idf.py set-target esp32s3
    idf.py build
</pre>

4. Put the firmware.bin in distrib server

<pre>
    make cp_distrib # In dev/target
    docker cp firmware.bin ota_server:/usr/share/nginx/html/firmware.bin
</pre>

5. Start access point
<pre>
    make cp_distrib # In dev/server/access_point
    make run
</pre>

6. Flash the target

<pre>
    make enter_idf # In dev/target
    idf.py -p /dev/ttyUSB0 flash
    idf.py monitor
</pre>

