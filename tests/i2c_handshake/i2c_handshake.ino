//    Automatic Handshake Over I2C/Qwiic For Arduboy Mini Example Code
//  By Jonathan Holmes (crait)
//
//  Website:    http://www.crait.net/
//  Twitter:    http://www.twitter.com/crait
//  Bluesky:    http://crait.bsky.social/
//
//  This code is provided so that you can test two Arduboy Mini units communicating over
//  an I2C/Qwiic cable. Most solutions I have found requires both users to have different
//  versions of their game to communicate so that the first version can be the primary unit
//  (which controls the communication between both) and the second version can be the secondary
//  unit (which is only used to display the results and to read button presses). However,
//  with my code, you can use the same code on both Arduboy Mini units. The units will
//  communicate together and automatically determine which one is the primary unit and which
//  one is the secondary, but allows the same code for sending and receiving data. This code
//  is also set up so that sending over data also sends over a 'command' to the other unit
//  to control the connection, meaning that you can easily tell both Arduboy units to
//  disonnect.
//
//  License:
//  You can use this code for whatever you want, but don't re-release it as your own code.
//  If you make a game with this code, you don't have to credit me... But, I do think it
//  would be cool if you tag me somehow and let me know that you used the code. I want to
//  see what you made. :)

#include <Arduboy2.h>
#include <Wire.h>

Arduboy2 arduboy;

//  For both Arduboy Mini units to communicate with each other, they must have their own
//  'address' that messages need to be sent to from the other unit. When starting this
//  program, though, the default address is just 0x00. This is the address that both
//  units want to communicate on when they are trying to initiate the handshake.
#define INITIAL_ADDRESS                         0x00
//  This Arduboy unit's address
uint8_t owner_address = INITIAL_ADDRESS;
//  The other Arduboy unit's address
uint8_t partner_address = INITIAL_ADDRESS;

//  A series of commands are used to automatically connect and disconnect the units,
//  establishing an automatic handshake. You can add your own commands if you want
//  to add more features.
#define TRANSMISSION_COMMAND_UNKNOWN            0
//  Used during the handshake to tell the other Arduboy unit that this is the primary unit
#define TRANSMISSION_COMMAND_SET_PRIMARY        10
//  Used during the handshake to tell the other Arduboy unit that this is the secondary unit
#define TRANSMISSION_COMMAND_SET_SECONDARY      20
//  Tells the other unit to disconnect
#define TRANSMISSION_COMMAND_DISCONNECT         30
//  Tells the other unit that data is being sent and that it should be read
#define TRANSMISSION_COMMAND_READ_DATA          40
//  The command that is received from the other unit is stored in received_command.
//  In your code, you do not have to make this a global variable. I just did so
//  because I want to show all the data to you, for testing purposes.
uint8_t received_command = TRANSMISSION_COMMAND_UNKNOWN;

//  Used to store the received data from the other unit.
//  This is only global so that we can display the data to you.
uint8_t data_received = 0;
//  Used to store the data that will be sent the other unit.
//  When you make your own game, you don't need to use this if you don't want to.
uint8_t data_to_send = 0;

//  There are 4 connection states that the Arduboy can be in
enum class Connection : uint8_t {
    //  Used when there is no connection at all
    Disconnected,
    //  Used when looking for another Arduboy. This will be the primary unit
    Waiting,
    //  Used when the other Arduboy wants to be the primary unit, so this will be secondary
    Responding,
    //  Used once the primary-secondary relationship is established
    Connected
};

//  The status of the connection is stored into this variable
Connection connection_status = Connection::Disconnected;

//  Whenever we want to transmit data to the other unit, we can call this function to start
//  a transmission with the partner's address, then send over a given command and data!
void transmit_data(uint8_t command, uint8_t data) {
    Wire.beginTransmission(partner_address);
    Wire.write(command);
    Wire.write(data);
    Wire.endTransmission();
}

//  If there is an established connection (connection_status is Connected), then this function
//  will be called. The parameter, count, is how many bytes are to be received. This can be
//  ignored in our case.
//  The first byte is read as the received command from the other Arduboy unit and different
//  code can be used for each command. If you want to add your own commands, you can add them
//  in the switch statement. Afterwards, any leftover data in the buffer is discarded.
//
//  TRANSMISSION_COMMAND_READ_DATA: The data is read and stored in data_received for later
//  TRANSMISSION_COMMAND_DISCONNECT: This tells the Arduboy that the other wants to disconnect
void receive_data(uint8_t count) {
    received_command = Wire.read();
    switch(received_command) {
        case TRANSMISSION_COMMAND_READ_DATA:
            data_received = Wire.read();
            break;
        case TRANSMISSION_COMMAND_DISCONNECT:
            disconnect();
            break;
    }
    Wire.flush();
}

//  This function will be called by both Arduboy units at different times during the
//  handshake process. (This is a callback function, so the count parameter can be ignored.)
//  This function should only be called with the TRANSMISSION_COMMAND_SET_PRIMARY or
//  TRANSMISSION_COMMAND_SET_SECONDARY. When this happens, the partner's address is received
//  and stored. If the partner unit's address is the same as this unit's address, then this
//  unit's address will be increments to ensure that they are different. Additionally, the
//  receive_data() callback will be enabled when data is received.
//  If the unit is the primary unit, then the conenction is established. If the unit is
//  the secondary unit, then it needs to respond with its own address to the primary unit.
void receive_partner(uint8_t count) {
    uint8_t new_partner_address = INITIAL_ADDRESS;
    received_command = TRANSMISSION_COMMAND_UNKNOWN;
    received_command = Wire.read();
    if(received_command != TRANSMISSION_COMMAND_SET_PRIMARY && received_command != TRANSMISSION_COMMAND_SET_SECONDARY) {
        Wire.flush();
        return;
    }

    if(received_command == TRANSMISSION_COMMAND_SET_PRIMARY) {
        connection_status = Connection::Responding;
    }
    new_partner_address = Wire.read();
    if(new_partner_address == owner_address) {
        owner_address++;
    }
    partner_address = new_partner_address;
    Wire.onReceive(receive_data);
    Wire.end();
}

//  This empty function will be used as the callback when no connection is established. That
//  means that the 
void empty_callback(uint8_t count) { }

//  This variable is used to show the progress when searting for another Arduboy.
//  You can remove this from your own code. :)
uint8_t dot_count = 0;

//  This function displays all of our variables and information about the connection.
//  You can also remove this from your own code, but it also is a good example of how to
//  handle your own game's code by checking the connection_status with a switch statement.
void display_text() {
    arduboy.print(F("Owner ID: "));
    arduboy.print(owner_address);
    arduboy.print(F("\nPartner ID: "));
    arduboy.print(partner_address);
    arduboy.print(F("\nReceived Command: "));
    arduboy.print(received_command);
    arduboy.print(F("\nReceived Data: "));
    arduboy.print(data_received);

    switch(connection_status) {
        case Connection::Disconnected:
            arduboy.print(F("\n\n\nStatus: Disconnected\n"));
            arduboy.print(F("A: Connect"));
            break;
        case Connection::Waiting:
            dot_count++;
            if(dot_count > 4) {
                dot_count = 0;
            }
            arduboy.print(F("\n\n\nStatus: Searching."));
            for(uint8_t i = 0; i < dot_count; i++) {
                arduboy.print(F("."));
            }
            arduboy.print(F("\n"));
            break;
        case Connection::Connected:
            arduboy.print(F("\nData To Send: < "));
            arduboy.print(int(data_to_send));
            arduboy.print(F(" > \n"));
            arduboy.print(F("A: Send Data\n"));
            arduboy.print(F("Status: CONNECTED\n"));
            break;
    }
    
    if(connection_status != Connection::Disconnected) {
        arduboy.print(F("B: Disconnect"));
    }
}

//  Whenever we want to disconnect this Arduboy unit, we can called this function to
//  reset our variables and telling the Arduboy to ignore any data that it receives
void disconnect() {
    connection_status = Connection::Disconnected;
    partner_address = INITIAL_ADDRESS;
    received_command = TRANSMISSION_COMMAND_UNKNOWN;
    Wire.flush();
    Wire.end();
    Wire.onReceive(empty_callback);
}


//  Whenever the connection status is Disconnected, you can start a connection with this
//  function. The Arduboy will try to start communication on the default address and begin
//  waiting for the partner to send their address. This Arduboy will also send over its
//  address and tell the other unit that this Arduboy is the primary unit.
void connect() {
    connection_status = Connection::Waiting;
    Wire.begin(INITIAL_ADDRESS);
    Wire.onReceive(receive_partner);
    transmit_data(TRANSMISSION_COMMAND_SET_PRIMARY, owner_address);
}

//  Once your Arduboy tries to connect, it needs to wait for the other Arduboy to send its
//  own address. Call this function while waiting for the partner address that is handled in
//  the receive_partner() callback function. Once the partner's address is set, then the
//  connection will be established on this Arduboy's address.
void wait_for_connection() {
    if(partner_address != INITIAL_ADDRESS) {
        connection_status = Connection::Connected;
        Wire.begin(owner_address);
    }
}

//  Once the primary unit tells the other Arduboy its address, the other Arduboy will be
//  the secondary unit. If this Arduboy is the secondary unit, it needs to respond to the
//  primary unit with its own address. Afterwards, we can assume everything went well and
//  consider the connection established, setting the connection_status to Connected!
void respond_to_primary() {
    connection_status = Connection::Connected;
    Wire.end();
    Wire.begin(INITIAL_ADDRESS);
    Wire.beginTransmission(INITIAL_ADDRESS);
    Wire.write(TRANSMISSION_COMMAND_SET_SECONDARY);
    Wire.write(owner_address);
    Wire.endTransmission();
    Wire.end();
    Wire.begin(owner_address);
    Wire.onReceive(receive_data);
}

//  By default, all Arduboy games need this setup() function when the game starts
void setup() {
    arduboy.boot();
    arduboy.initRandomSeed();
    arduboy.setFrameRate(25);

    //  When starting the game, a random owner address is selected
    do {
        owner_address = random(UINT8_MAX - 1);
    } while(owner_address == INITIAL_ADDRESS);

    //  This code is necessary to enable communication through the I2C/Qwiic port
    power_twi_enable();
}

//  By default, all Arduboy games need this loop() function to handle the game's main loop.
//  This is where the logic for the handshaking code lives. For your own code, you can remove
//  it and put it somewhere else if you wanted to. I think handling the connection_status at
//  before the game's logic because communication with the other Arduboy should dictate what
//  happens in your own game, in most circumstance.
void loop() {
    if(!arduboy.nextFrame()) {
        return;
    }
    arduboy.clear();
    arduboy.pollButtons();

    //  Different code will execute, depending on the connection status... This code is used to
    //  handle moving from one status to another!
    switch(connection_status) {
        case Connection::Disconnected:
            connect();
            break;
        case Connection::Waiting:
            wait_for_connection();
            break;
        case Connection::Responding:
            respond_to_primary();
            break;
        case Connection::Connected:
            if(data_to_send == 0) {
                data_to_send = 42;
                transmit_data(TRANSMISSION_COMMAND_READ_DATA, data_to_send);
            }

            break;
    }
    
    //  If the connection is established or is in the process of being established, you can
    //  cancel the process or set everything to disconnect with the A button. When trying to
    //  disconnect, don't forget to tell the other Arudboy to disconnect, too!
    if(connection_status != Connection::Disconnected) {
        if(arduboy.justReleased(B_BUTTON)) {
            transmit_data(TRANSMISSION_COMMAND_DISCONNECT, 0);
            disconnect();
        }
    }

    //  Draw the text and variables and states and stuff :)
    display_text();

    arduboy.display();

    if(data_received == 42)
    {
        UEDATX = 'P';
        for(;;) arduboy.idle();
    }
}
