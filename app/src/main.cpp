#include <iostream>

#include "types.hpp"
#include "consts.hpp"
#include "instruction_memory.hpp"
#include "control_unit.hpp"
#include "program_counter.hpp"
#include "register.hpp"
#include "register_file.hpp"
#include "ALU_SH.hpp"
#include "data_memory.hpp"

int main(int argc, char *argv[])
{
    Emulator::Types::control_lines_t control_lines;
    Emulator::Types::BUSES_t BUS;

    // Loads an instruction memory object with the file specified by the first command-line argument
    InstructionMemory Rom(argv[1]);
    ControlUnit Decoder;
    ProgramCounter Program_counter;
    Register TR0;
    Register TR1;
    Register TR2;
    ALU_SH ALU;
    RegisterFile Register_file;
    DataMemory Data_memory;


    // Initialize BUS lanes to 0 (More will be added)
    BUS.IMM_TO_PC = 0;
    BUS.PC_to_IM  = 0;

    // Initialize basic variables
    int phase = 0; // either 0 or 1
    int clock = 0; // either 0 or 1

    bool running = true;
    // Not used by CPU, just for diagnostics
    int counter = 0;

    while(running)
    {
        /*
        *       Clock 0, PHASE 0
        */
            clock = 0;
            phase = 0;
            std::cout << "\n--------------- " << "New instruction (" << counter << ")" << " ---------------" << std::endl;

            // Get current instruction and print its properties
            DecodedInst current_inst = Rom.get_decoded_inst(BUS.PC_to_IM >> 2);
            current_inst.print_info();
            // Decode current instruction into control lines and print them
            control_lines = Decoder.update_control_signals(current_inst.opcode, control_lines);
            Decoder.print_control_signals(control_lines);

            control_lines.funct3 = current_inst.func3;
            control_lines.i = (bool)(((current_inst.instruction) >> 30) & 1);

            // Send Immediate to Writeback if needed
            if (control_lines.IMM_TO_WB == 1) { BUS.WB = current_inst.imm; }
            // Immediate to Program counter
            BUS.IMM_TO_PC = current_inst.imm;

            Register_file.read(BUS, current_inst.rs1, current_inst.rs2);

       /*
        *       Clock 1, PHASE 0
        */
            clock = 1;
            phase = 0;

            BUS = Program_counter.update_BUS(BUS, control_lines, phase, clock);

            // Store register output to temporary register 0 and 1
            BUS.TR0_TO_ALU0 = TR0.update(BUS.RF0_TO_TR0);
            BUS.TR1_TO_RAMD = TR1.update(BUS.RF1_TO_TR1);
            // Update temporary register 2
            BUS.TR2_TO_MUX = TR2.update(BUS.PC_TO_TR2);


            std::cout << "TR0: 0x" << std::hex << BUS.TR0_TO_ALU0 << "\t";
            std::cout << "TR1: 0x" << std::hex << BUS.TR1_TO_RAMD << "\t";
            std::cout << "TR2: 0x" << std::hex << BUS.TR2_TO_MUX << std::endl;


            // MUX (TR1 / Immediate) to ALU source 1. Depends on ALU_SRC
            if (control_lines.ALU_SRC == 0) { BUS.ALU_MUX_TO_ALU1 = BUS.TR1_TO_RAMD; }
            else { BUS.ALU_MUX_TO_ALU1 = current_inst.imm; }

            // // MUX (WB / TR2) as data to destination register. Depends on PC_SRC 0 or 1
            // if ((control_lines.PC_SRC_0 | control_lines.PC_SRC_1) == 0) { BUS.WB_TO_RF = BUS.WB; }
            // else { BUS.WB_TO_RF = BUS.PC_TO_TR2; }

            ALU.update(&BUS, control_lines);
            std::cout << "ALU output: 0x" << std::hex << BUS.ALU_TO_DM << std::endl;

            // Send ALU output to Writeback if needed
            if (control_lines.ALU_TO_WB == 1) { BUS.WB = BUS.ALU_TO_DM; }

       /*
        *       Clock 0, PHASE 1
        */
            clock = 0;
            phase = 1;

            if (control_lines.STR_TO_RAM == 1)
            {
                Data_memory.store(&control_lines, &BUS);
            }
            if (control_lines.RAM_TO_WB == 1)
            {
                Data_memory.load(&control_lines, &BUS);
            }

            // MUX (WB / TR2) as data to destination register. Depends on PC_SRC 0 or 1
            if ((control_lines.PC_SRC_0 | control_lines.PC_SRC_1) == 0) { BUS.WB_TO_RF = BUS.WB; }
            else { BUS.WB_TO_RF = BUS.PC_TO_TR2; }

       /*
        *       Clock 1, PHASE 1
        */
            clock = 1;
            phase = 1;

            // Store to register file in needed
            if (control_lines.STR_TO_RF == 1) { Register_file.write(BUS, current_inst.rd);  }
            Register_file.Test();
            

            BUS = Program_counter.update_BUS(BUS, control_lines, phase, 0);

            if (current_inst.instruction == Emulator::Consts::END_INSTRUCTION)
            {
                running = false;
                std::cout << "\nEnd of program." << std::endl;
                Data_memory.print();
                break;
            }

        if (counter == 0x13) {running = false;}
        counter++;
    }    


    // Main loop; one loop cycle = one clock period, PHASE = 2*clock period
    // while (running)
    // {
    //     /*
    //      *       Clock is 0 for PHASE 0 or 1
    //      */
    //     clock = 0;
    //     std::cout << "\nPhase: " << phase << std::endl;

    //     int current_address = BUS.PC_to_IM;

    //     // Get current instruction and print its properties
    //     DecodedInst current_inst = Rom.get_decoded_inst(current_address >> 2);
    //     current_inst.print_info();
      
    //     // Send Immediate to Writeback
    //     if (control_lines.IMM_TO_WB == 1) { BUS.WB = current_inst.imm; }
        
    //     //Test Register File with a read and write
    //     Register_file.write(BUS, current_inst.rd);
    //     Register_file.read(BUS, current_inst.rs1,current_inst.rs2);

    //     // Stop when the instruciton jumps to itself (Had to put it here and not at the end bcs segmentation fault when it tried to read non existing instruction)
    //     if (current_inst.instruction == Emulator::Consts::END_INSTRUCTION)
    //     {
    //         running = false;
    //         std::cout << "\nEnd of program." << std::endl;
    //         break;
    //     }

    //     // Decode current instruction into control lines and print them
    //     control_lines = Decoder.update_control_signals(current_inst.opcode, control_lines);
    //     Decoder.print_control_signals(control_lines);

    //     // Update program counter, in PHASE 1 it will load new value. Needs to be last
    //     BUS = Program_counter.update_BUS(BUS, control_lines, phase, clock);

    //     // Register file:

    //     // Updates the ALU
    //     ALU.update(&BUS, &control_lines);
        

    //     /*
    //      *       Clock is 1 for PHASE 0 or 1
    //      */
    //     clock = 1;

    //     // Update temporary registers
    //     if (phase == 0 && clock == 1)
    //     {
    //         // TR0: TODO
    //         // TR1: TODO
    //         BUS.TR2_TO_MUX = TR2.update(BUS.PC_TO_TR2);
    //     }

    //     // Updates the ALU again
    //     ALU.update(&BUS, &control_lines);

    //     // Send ALU output to Writeback
    //     if (control_lines.ALU_TO_WB == 1) { BUS.WB = BUS.ALU_TO_DM; }

    //     // Change phases (from 0 to 1, from 1 to 0)
    //     phase = (phase + 1) % 2;
    // }
    // Register_file.Test();
} // main