# Ring Ping

This code is designed to ping all nodes on the nlnog ring, and store the results in an influx database

The configuration file in config/sysconfig.json will need to be modified to the users needs

# Compilation

cmake -G Ninja .
ninja

Note: This requires libjson-c-dev and libcurl4-openssl-dev be installed in order to compile

