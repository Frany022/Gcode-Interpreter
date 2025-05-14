#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "xil_printf.h"
#include "xbasic_types.h"
#include "xparameters.h"
#include "xuartps_hw.h" // XUARTPS_FIFO_OFFSET
#define UART_BASEADDR XPAR_XUARTPS_0_BASEADDR //supposedly the base address or, _1_ for using second UART connection
#define is_valid(data) ((data & (1<<8)) > 0)
/*
TODO:
do something about different file types
step directions
maybe feedback from FPGA
*/
const int STEP_PER_MM_X = 1;
const int STEP_PER_MM_Y = 1;
const int STEP_PER_MM_Z = 1;
//ports:
#define FLAG_X (1 << 0)
#define FLAG_Y (1 << 1)
#define FLAG_Z (1 << 2)
#define FLAG_F (1 << 3)

typedef struct{
    uint8_t command;
    uint8_t flags;
    int32_t x, y, z;
    uint16_t feedrate;
}__attribute__((packed)) BinaryFlags;

typedef enum{
    //G17 = 17, //select XY plane
    //G90 = 90, //absolute positioning
    G28 = 28, //autohome, base position
    G00 = 0, //rapid movement, move to position at max speed
    G01 = 1,  //Move to position with feed rate
    M30 = 30,  //stop program
}GcodeType;

typedef struct{
    GcodeType command;
    int x, y, z;
    int feedrate;
    int has_x, has_y, has_z, has_f;
}GcodeCommand;

GcodeType parser(const char* line, GcodeCommand* cmd)
{
    memset(cmd, 0, sizeof(GcodeCommand)); //zeros out everything
    char buffer[128];
    strncpy(buffer, line, sizeof(buffer) -1);
    buffer[sizeof(buffer) -1] = '\0';

    char* token = strtok(buffer, " \t\n");
    while(token != NULL)
    {
        int gnumber = atoi(&token[1]);
        switch(token[0])
        {
            case 'G':
                switch(gnumber)
                {
                    case 28: cmd->command = G28; break;
                    case 1: cmd->command = G01; break;
                    case 0: cmd->command = G00; break;
                    default: cmd->command = -1; break;
                }
                break;
            case 'X':
                cmd->x = atoi(&token[1]);
                cmd->has_x = 1;
                break;
            case 'Y':
                cmd->y = atoi(&token[1]);
                cmd->has_y = 1;
                break;
            case 'Z':
                cmd->z = atoi(&token[1]);
                cmd->has_z = 1;
                break;
            case 'F': //probably this part is not needed
                cmd->feedrate = atoi(&token[1]);
                cmd->has_f = 1;
                break;

        }
        token = strtok(NULL, " \t\n");
    }
}


uint8_t compute_flags(const GcodeCommand *cmd)
{
    uint8_t flags = 0;
    if(cmd->has_x) flags |= FLAG_X;
    if(cmd->has_y) flags |= FLAG_Y;
    if(cmd->has_z) flags |= FLAG_Z;
    if(cmd->has_f) flags |= FLAG_F;
    return flags;
}

void steps(const GcodeCommand* cmd, BinaryFlags* flags)
{
    flags->command = (uint8_t)cmd->command;
    flags->flags = compute_flags(cmd);
    flags->x = cmd->has_x ? cmd->x * STEP_PER_MM_X : 0;
    flags->y = cmd->has_y ? cmd->y * STEP_PER_MM_Y : 0;
    flags->z = cmd->has_z ? cmd->z * STEP_PER_MM_Z : 0;
    flags->feedrate = cmd->has_f ? cmd->feedrate : 0;
}




int main()
{
    GcodeCommand cmd;
    BinaryFlags flags;
    char line[128];
    char* filepath = "test.gcode";
    FILE* fp = fopen(filepath, "r");
    int last_x = -1, last_y = -1, last_z = -1, last_feedrate = -1; //feedrate probably not needed but its there just in case
    if(!fp)
    {
        perror("File can't be opened");
        return 1;
    }
    while(fgets(line, sizeof(line), fp))
    {
        if(line[0] == ';' || line[0] == '\n') continue;
        parser(line,&cmd);
        steps(&cmd,&flags);

        //checking if any coordinates changed so we don't send the same data:
        if((cmd.has_x && cmd.x != last_x) || (cmd.has_y && cmd.y != last_y) || (cmd.has_z && cmd.z != last_z) || (cmd.has_f && cmd.feedrate != last_feedrate))
        {
            last_x = cmd.has_x ? cmd.x : last_x;
            last_y = cmd.has_y ? cmd.y : last_y;
            last_z = cmd.has_z ? cmd.z : last_z;
            last_feedrate = cmd.has_f ? cmd.feedrate : last_feedrate;

            uint8_t* ptr = (uint8_t)&flags;

            for(int i = 0; i < sizeof(BinaryFlags); i++)
            {
                while(XUartPs_IsTransmitFull(UART_BASEADDR));
                XUartPs_WriteReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET, ptr[i]); //sending raw bytes directly not the memory-mapped thing
            }

        }
        /* testing
        printf("parsed: G%02d", cmd.command);
        if(cmd.has_x) printf(" x=%d ",cmd.x);
        if(cmd.has_y) printf(" y=%d ",cmd.y);
        if(cmd.has_z) printf(" z=%d ",cmd.z); //probably not needed
        if(cmd.has_f) printf(" f=%d ", cmd.feedrate); //probably not needed
        printf("\n");
        printf("Parsed: G%2d and flags: 0x%02X\n",flags.command,flags.flags);
        */
    }
    fclose(fp);
    return 0;

}
