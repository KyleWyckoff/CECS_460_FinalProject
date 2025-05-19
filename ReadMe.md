**CECS460 Final Project Submission**

The following repository contains multiple files that make up our final submission for the project.  First, folders hdl, ip and ui as well as file design_1.bd 
make up our onboard simulation, shown in the attached videos.  These files were taken from a premade Diligent Project from https://digilent.com/reference/programmable-logic/zybo-z7/demos/pcam-5c?__cf_chl_tk=CGgEm80u1jbSwIDMqUf6DmvZ5jsff3G9kcO7mF3d1qI-1746829306-1.0.1.1-XrhnqqZjJJRRrytpRYdBTNJKLgQfs41lY3kqC8PwTZ0 

Second, the OpenCV files attached aimed to modify the project's camera output in an image processing direction.  Our intention was that if we could perform a 
small-scale modification like applying a filter through an OpenCV, more advanced features such as ai image detection or app quality filters could  be added to 
the project.  Unfortunately, working with the openCV and Linux proved to be too difficult for us to implement the onboard, bare-metal implementation and is an improvement
for the future that can be made on this project.  

Third, the requirements of the project also ask for a SystemC design that mimics the functionality of our onboard design.  The modules mimic the project, with a HDMI_RX 
input being driven by a driver module, that communicates to the CPU.  From there, the CPU connects to the AXI_Bus which in turn connects to the Memory module, finally 
connecting to the HDMI_TX module used as our output to the monitor.  Console print statements are provided during each step of the simulation to provide accurate descriptions of 
what is occurring.
