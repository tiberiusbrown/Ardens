import sys, getopt

def main(argv):
    input_file      = ''
    array_name      = ''
    output_file     = ''
    output_h_file   = ''

    try:
        opts, args = getopt.getopt(argv, "hi:a:o:",["ifile=","len=","arrayname=","ofile="])
    except:
        print("bin2c.py -i <binary file to convert> -a <C array name> -o <output C file>")
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print("bin2c.py -i <binary file to convert> -a <C array name> -o <output C file>")
            sys.exit(0)
        elif opt in ("-i", "--ifile"):
            input_file = arg
        elif opt in ("-a", "--arrayname"):
            array_name = arg
        elif opt in ("-o", "--ofile"):
            output_file = arg
    
    # Create the file name for the header file by removing the .c extension and adding .h
    output_h_file = output_file[:-2] + ".h"
    print("Converting: " + input_file + " to C array " + array_name + " in files " + output_file + " and " + output_h_file)

    # Open the binary file for reading
    try:
        infile = open(input_file, "rb")
    except:
        print("Couldn't open input file: " + input_file)
        sys.exit(2)

    # Read the contents of the file
    # As this is written to convert binary images for embedded targets the file size will be a few MB at most
    bin_file_contents = infile.read()

    # Close the file
    infile.close()

    # Write the C source file
    try:
        outfile = open(output_file, "w")
    except:
        print("Couldn't open output file: " + output_file)
        sys.exit(2)

    # Convert the binary contents into C hex values
    outfile.write("const unsigned char " + array_name + "[" + str(len(bin_file_contents)) + "] = {")
    for x in range(len(bin_file_contents)):
        if (x % 16 == 0):
            outfile.write("\n")
            outfile.write("\t")
        outfile.write("0x" + hex(bin_file_contents[x])[2:].zfill(2) + ", ")

    outfile.write("};")
    outfile.close()

    # Write the header file
    try:
        outfile = open(output_h_file, "w")
    except:
        print("Couldn't open output file: " + output_h_file)
        sys.exit(2)

    outfile.write("#ifndef _" + array_name.upper() + "_BIN2C_HEADER\n")
    outfile.write("#define _" + array_name.upper() + "_BIN2C_HEADER\n")
    outfile.write("\n")
    outfile.write("extern const unsigned char " + array_name + "[" + str(len(bin_file_contents)) + "];\n")
    outfile.write("\n")
    outfile.write("#endif")
    outfile.close()

    print("File converted")

    sys.exit(0)


if __name__ == "__main__":
    main(sys.argv[1:])
