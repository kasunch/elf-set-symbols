## elf-set-symbols

This tool provides an easy way to set symbols in an ELF file.

#### Compiling
Compilation requires standard C++ development libraries installed.

    make

It is possible to cross-compile the tool for ARM platforms running Linux as follows.

    make ARCH=arm

#### Usage
    Usage: elf-set-symbols -i <input elf file> -o <output elf file> [OPTIONS]

    -i, --input                          Input ELF file
    -o, --output                         Output ELF file
    
    -s, --set-sym "<name> = <value>"     Specify a symbol and its value to be changed
                                         The value should be in hexadecimal and big-endian
                                         Ex: -s "NODE_ID=0100" -s "DEBUG=01"
    
    -a, --set-at "<address> = <value>"   Specify an address and value to be changed
                                         The address should be in hexadecimal and little-endian
                                         The value should be in hexadecimal and big-endian
                                         Ex: -a "0x0027FFD7=F1" -a "0x0027FFD4=FF"

    -h, --help                           Display this help