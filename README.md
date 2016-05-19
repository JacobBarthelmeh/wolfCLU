#wolfCLU

This is the wolfSSL:  Command Line Utility (wolfCLU).

##wolfSSL Install

To use this feature, please configure and install wolfssl with the following commands:

    ./configure --enable-pwdbased --enable-opensslextra && make && make check

If that succeeds, run:

    sudo make install

`--enable-pwdbased` is for password based encryption allowing the user
to specify a unique password known only to him/her self and the
recipient of the encrypted file.

`--enable-opensslextra` provides utility for a hex to binary conversion of
hexidecimal values.

Additional features that can be included when configuring wolfssl for
encryption or decryption are:

        --enable-camellia
        --enable-des3
        --enable-blake2
        --enable-sha512
        --enable-fortress

##wolfCLU Install

After wolfssl is installed, install wolfCLU.  In the directory
`wolfssl-examples/wolfCLU` enter the following commands:

    ./autogen.sh
    ./configure
    make
    sudo make install

Now you should be able to use the wolfssl command line tool.  To verify type:

    wolfssl -h

If everything worked, you should see the wolfssl help page.

Thank you and have fun!
