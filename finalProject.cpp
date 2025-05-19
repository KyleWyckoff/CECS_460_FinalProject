#include <systemc.h>
#include <iostream>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/multi_passthrough_initiator_socket.h>


int main() {
    std::cout << "Test file compiled successfully!" << std::endl;
    return 0;
}


SC_MODULE(HDMI_RX_Driver) {
    tlm_utils::simple_initiator_socket<HDMI_RX_Driver> socket; // Initiator socket

    SC_CTOR(HDMI_RX_Driver) : socket("socket") {
        SC_THREAD(run); // Thread to send transactions
    }

    void run() {
        tlm::tlm_generic_payload trans;
        unsigned char pixel_data = 128; // Example pixel data
        sc_time delay = SC_ZERO_TIME;

        // Configure the transaction
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0); // Address is not used in HDMI_RX
        trans.set_data_ptr(&pixel_data);
        trans.set_data_length(1);

        std::cout << "HDMI_RX_Driver: Sending pixel data to HDMI_RX." << std::endl;

        // Send the transaction to HDMI_RX
        socket->b_transport(trans, delay);

        // Check the response status
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "HDMI_RX_Driver: Failed to send data to HDMI_RX!" << std::endl;
        } else {
            std::cout << "HDMI_RX_Driver: Data successfully sent to HDMI_RX." << std::endl;
        }
        delay += sc_time(10, SC_NS);

    }
};

SC_MODULE(HDMI_RX)
{
    tlm_utils::simple_target_socket<HDMI_RX> socket; // Target socket
    sc_event data_received_event;                    // Event to notify CPU

    // Constructor
    SC_CTOR(HDMI_RX) : socket("socket")
    {
        socket.register_b_transport(this, &HDMI_RX::b_transport); // blocking transport method for LT model
    }
    // Blocking Transport Method
    void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay)
    {
        unsigned char *pixel_data = trans.get_data_ptr();          // Get pixel data from dummy payload
        std::cout << "HDMI RX: Received pixel data." << std::endl; // msg sent to console
        std::cout << "Pixel data: " << *pixel_data << std::endl;   // msg sent to console
        delay += sc_time(10, SC_NS);
        data_received_event.notify(SC_ZERO_TIME);
        // 10ns delay to follow LT model
    }
};

// CPU module responseible for receiving data from the AXI bus and sending it to the HDMI TX port
SC_MODULE(CPU){
    tlm_utils::simple_initiator_socket<CPU> socket; // Initiator socket
    sc_event *data_received_event;                  // Pointer to HDMI_RX's event

    // Constructor
    SC_CTOR(CPU) : socket("socket"){
        SC_THREAD(run);
    }

    void run(){
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        std::cout << "Initiator: Sending transaction." << std::endl;
        socket->b_transport(trans, delay); // send transaction to target
        // Propagate delay
        delay += sc_time(10, SC_NS);

        // Wait for notification from HDMI_RX
        if (data_received_event)
        {
            std::cout << "CPU: Waiting for data from HDMI_RX..." << std::endl;
            data_received_event->notify(SC_ZERO_TIME);
            std::cout << "CPU: Notified by HDMI_RX that data was received!" << std::endl;
        }
    }
};

// AXI Bus module used to connect CPU to hdmi tx port and Memory to hdmi rx port
SC_MODULE(AXI_Bus)
{
    tlm_utils::multi_passthrough_target_socket<AXI_Bus> target_socket;
    tlm_utils::multi_passthrough_initiator_socket<AXI_Bus> initiator_socket;
    // Contructor
    SC_CTOR(AXI_Bus) : target_socket("target_socket"), initiator_socket("initiator_socket")
    {
        target_socket.register_b_transport(this, &AXI_Bus::b_transport);
    }

    void b_transport(int id, tlm::tlm_generic_payload &trans, sc_time &delay)
    {
        // Forward the transaction to all connected targets
        for (unsigned int i = 0; i < initiator_socket.size(); ++i)
        {
            initiator_socket[i]->b_transport(trans, delay);
        }
        // Propagate delay
        delay += sc_time(5, SC_NS);
        std::cout << "AXI_Bus: Transaction forwarded to initiator." << std::endl; // msg sent to console
    }
};

// Memory module responsible for receiving data from the CPU and sending it to the HDMI TX port
SC_MODULE(Memory)
{
    tlm_utils::simple_target_socket<Memory> socket; // Target socket
    int mem[256];                                   // Array to hold current memory data
    // Constructor
    SC_CTOR(Memory) : socket("socket")
    {
        socket.register_b_transport(this, &Memory::b_transport); // blocking transport method for LT model
    }
    // Blocking transport method called when CPU sends transaction to memory
    void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay)
    {
        unsigned char *data = trans.get_data_ptr(); // get data pointer from CPU
        unsigned int addr = trans.get_address();    // get address from CPU

        if (data == nullptr)
        {
            std::cerr << "Target: Data pointer is null." << std::endl;  // msg sent to console
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); // set response status to generic error
            return;
        }

        if (addr >= 256)
        {
            std::cerr << "Target: Address out of range." << std::endl;  // msg sent to console
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); // set response status to address error
            return;
        }

        if (trans.is_write())
        {
            std::cout << "Target: Write transaction received." << std::endl; // msg sent to console
            mem[trans.get_address()] = *data;                                // write data address to mem array to access for write request
            std::cout << "Data written to memory: " << *data << std::endl;   // msg sent to console
        }

        else
        {
            std::cout << "Target: Read transaction received." << std::endl; // msg sent to console
            *data = mem[trans.get_address()];                               // read data from mem array after a read request
            std::cout << "Data read from memory: " << *data << std::endl;   // msg sent to console
        }

        delay += sc_time(10, SC_NS); // add a 10 ns delay for simulation
    }
};

SC_MODULE(HDMI_TX){
    tlm_utils::simple_initiator_socket<HDMI_TX> socket; // Initiator socket
    // Constructor
    SC_CTOR(HDMI_TX) : socket("socket")
    {
        SC_THREAD(run);
    }
    void run()
    {
        while (true)
        {
            std::cout << "HDMI_TX: Requesting data from memory at address 0." << std::endl;
            unsigned char data; // Buffer to store data from memory
            tlm::tlm_generic_payload trans;
            sc_time delay = SC_ZERO_TIME;

            // Send the transaction to memory
            socket->b_transport(trans, delay);

            // Check response status
            if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
            {
                std::cerr << "HDMI_TX: Failed to read data from memory!" << std::endl;
            }
            else
            {
                // Output the data to the console
                std::cout << "HDMI_TX: Data received from memory = " << static_cast<int>(data) << std::endl;
            }
            delay += sc_time(10, SC_NS);
        }
    }
};

int sc_main(int argc, char *argv[]){

    // HDMIRX -> CPU -> BUS -> MEMORY -> HDMITX

    CPU cpu("cpu");                           // create cpu object
    Memory memory("memory");                  // create memory object
    HDMI_RX_Driver hdmi_rx_driver("hdmi_rx_driver");               // create hdmi rx object

    HDMI_RX hdmi_rx("hdmi_rx");               // create hdmi rx object
    AXI_Bus bus("bus");                       // create bus object
    hdmi_rx_driver.socket.bind(hdmi_rx.socket); // Bind driver to HDMI_RX
    hdmi_rx.socket.bind(cpu.socket);          // bind cpu to hdmi rx port
    cpu.socket.bind(bus.target_socket);       // bind cpu to bus
    bus.initiator_socket.bind(memory.socket); // bind bus to memory
    // Pass the HDMI_RX event to the CPU
    cpu.data_received_event = &hdmi_rx.data_received_event;
 
    sc_core::sc_start();
    return 0;



}
