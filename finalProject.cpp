#include <systemc.h>
#include <iostream>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/multi_passthrough_initiator_socket.h>


SC_MODULE(HDMI_RX_Driver) {
    tlm_utils::simple_initiator_socket<HDMI_RX_Driver> socket; // Initiator socket (connects to HDMI_RX)
    SC_CTOR(HDMI_RX_Driver) : socket("socket") {
        SC_THREAD(run); // Thread to send transactions
    }
    void run() {
        tlm::tlm_generic_payload trans;
        unsigned char pixel_data = 123; //Pixel data
        sc_time delay = SC_ZERO_TIME;
        // Configure the transaction
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0); 
        trans.set_data_ptr(&pixel_data);
        trans.set_data_length(1);
        std::cout << "HDMI_RX_Driver: Sending pixel data to HDMI_RX." << std::endl;         //Msg to console indicating data was sent
        socket->b_transport(trans, delay);                                                  // Send the transaction to HDMI_RX
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {                          // Check the response status
            std::cerr << "HDMI_RX_Driver: Failed to send data to HDMI_RX!" << std::endl;    //Failed to send data
        } else {
            std::cout << "HDMI_RX_Driver: Data successfully sent to HDMI_RX." << std::endl; //Msg to console data was sent
        }
        delay += sc_time(10, SC_NS);
    }
};

SC_MODULE(HDMI_RX)
{
    tlm_utils::simple_target_socket<HDMI_RX> socket; // Target socket
    sc_event data_received_event;                    // Event to notify CPU
    SC_CTOR(HDMI_RX) : socket("socket"){                                     // Constructor
        socket.register_b_transport(this, &HDMI_RX::b_transport);            // blocking transport method for LT model
    }
    void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay){     // Blocking Transport Method
        unsigned char *pixel_data = trans.get_data_ptr();                    // Get pixel data from dummy payload
        if(pixel_data == nullptr){                                           //Error checking in case pixel data is null
            std::cerr << "HDMI_RX: Error recieving proper pixel data" << std::endl; 
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);      // Set error response
            return;
        }
        if(trans.get_data_length() != 1){                                    //Error checking if data length is valid
            std::cerr << "HDMI_RX: Error recieving improper pixel data length" << std::endl;
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);        // Set error response
            return;
        }
        std::cout << "HDMI RX: Received pixel data." << std::endl;           // msg sent to console
        std::cout << "Pixel data: " << *pixel_data << std::endl;             // msg sent to console
        delay += sc_time(10, SC_NS);                                         // 10ns delay to follow LT model
        data_received_event.notify(SC_ZERO_TIME);                            //Tell the CPU data was recieved
        trans.set_response_status(tlm::TLM_OK_RESPONSE);                     // Set successful response
    }
};

// CPU module responseible for receiving data from the AXI bus and sending it to the HDMI TX port
SC_MODULE(CPU){
    tlm_utils::simple_initiator_socket<CPU> socket; // Initiator socket
    sc_event *data_received_event;                  // Pointer to HDMI_RX's event
    SC_CTOR(CPU) : socket("socket"){    // Constructor
        SC_THREAD(run);
    }
    void run(){
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;                         //Delay
        std::cout << "Initiator: Sending transaction." << std::endl;            //Msg to console transaction was sent
        socket->b_transport(trans, delay);                                      // send transaction to target
        delay += sc_time(10, SC_NS);                                            // Propagate delay
        if (data_received_event){                                               // Wait for notification from HDMI_RX
            std::cout << "CPU: Waiting for data from HDMI_RX..." << std::endl;  //Send message to console that the cpu is waiting for a flag
            data_received_event->notify(SC_ZERO_TIME);                          // Notify the CPU that data was received                      
            std::cout << "CPU: Notified by HDMI_RX that data was received!" << std::endl;  //Flag was recieved (not sure of functionality)
        }
    }
};

// AXI Bus module used to connect CPU to bus and bus to memory
SC_MODULE(AXI_Bus){
    tlm_utils::multi_passthrough_target_socket<AXI_Bus> target_socket;
    tlm_utils::multi_passthrough_initiator_socket<AXI_Bus> initiator_socket;
    // Contructor
    SC_CTOR(AXI_Bus) : target_socket("target_socket"), initiator_socket("initiator_socket"){
        target_socket.register_b_transport(this, &AXI_Bus::b_transport);
    }
    void b_transport(int id, tlm::tlm_generic_payload &trans, sc_time &delay){
        unsigned int addr = trans.get_address();        // Determine the target based on the address
        unsigned int target_id = addr / 256;            // Example: Divide address space into 256-byte regions
        if (target_id < initiator_socket.size()){
            initiator_socket[target_id]->b_transport(trans, delay);
            std::cout << "AXI_Bus: Transaction forwarded to target " << target_id << "." << std::endl;
        }
        else{
            std::cerr << "AXI_Bus: Error - Address out of range." << std::endl;
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }
        delay += sc_time(5, SC_NS); // Example delay for the bus
    }
};

// Memory module responsible for receiving data from the CPU and sending it to the HDMI TX port
SC_MODULE(Memory){
    tlm_utils::simple_target_socket<Memory> socket;                          // Target socket
    int mem[256];                                                            // Array to hold current memory data
    SC_CTOR(Memory) : socket("socket"){                                      // Constructor

        socket.register_b_transport(this, &Memory::b_transport);             // blocking transport method for LT model
    }
    // Blocking transport method called when CPU sends transaction to memory
    void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay){
        unsigned char *data = trans.get_data_ptr();                          // get data pointer from CPU
        unsigned int addr = trans.get_address();                             // get address from CPU
        if (data == nullptr){
            std::cerr << "Target: Data pointer is null." << std::endl;       // msg sent to console
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);      // set response status to generic error
            return;
        }
        if (addr >= 256){
            std::cerr << "Target: Address out of range." << std::endl;       // msg sent to console
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);      // set response status to address error
            return;
        }
        if (trans.is_write()){
            std::cout << "Target: Write transaction received." << std::endl; // msg sent to console
            mem[trans.get_address()] = *data;                                // write data address to mem array to access for write request
            std::cout << "Data written to memory: " << *data << std::endl;   // msg sent to console
        }else{
            std::cout << "Target: Read transaction received." << std::endl;  // msg sent to console
            *data = mem[trans.get_address()];                                // read data from mem array after a read request
            std::cout << "Data read from memory: " << *data << std::endl;    // msg sent to console
        }
        delay += sc_time(10, SC_NS);                                         // add a 10 ns delay for simulation
    }
};

SC_MODULE(HDMI_TX){
    tlm_utils::simple_initiator_socket<HDMI_TX> socket;           // Initiator socket
    SC_CTOR(HDMI_TX) : socket("socket"){                          // Constructor
        SC_THREAD(run);
    }
    void run(){
        std::cout << "HDMI_TX: Requesting data from memory at address 0." << std::endl;
        unsigned char data;                                       // Buffer to store data from memory
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        socket->b_transport(trans, delay);                        // Send the transaction to memory for pixel data
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE){ // Check response status of memory
            std::cerr << "HDMI_TX: Failed to read data" << std::endl;
        } else{
            // Output the data to the console
            std::cout << "HDMI_TX: Data received from memory = " << static_cast<int>(data) << std::endl;
        }
        delay += sc_time(10, SC_NS);                               // Propagate delay  
    }
};

int sc_main(int argc, char *argv[]){
    // HDMIRXDRIVER -> HDMIRX -> CPU -> AXIBUS -> MEMORY -> HDMITX
    CPU cpu("cpu");                                             // create cpu object
    Memory memory("memory");                                    // create memory object
    HDMI_RX_Driver hdmi_rx_driver("hdmi_rx_driver");            // create hdmi rx object
    HDMI_TX hdmi_tx("hdmi_tx");                                 // create hdmi tx object
    HDMI_RX hdmi_rx("hdmi_rx");                                 // create hdmi rx object
    AXI_Bus bus("bus");                                         // create bus object
    hdmi_rx_driver.socket.bind(hdmi_rx.socket);                 // Bind driver to HDMI_RX
    hdmi_rx.socket.bind(cpu.socket);                            // bind cpu to hdmi rx port
    cpu.socket.bind(bus.target_socket);                         // bind cpu to bus
    bus.initiator_socket.bind(memory.socket);                   // bind bus to memory
    memory.socket.bind(hdmi_tx.socket);                         // bind memory to hdmi tx port
    cpu.data_received_event = &hdmi_rx.data_received_event;     // Pass the HDMI_RX event to the CPU
    sc_core::sc_start();                                        // Start sim         
    return 0;
}
