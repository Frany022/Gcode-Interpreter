#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//its only a parser so far
typedef enum{
    //G17 = 17, //select XY plane
    //G90 = 90, //absolute positioning
    G28 = 28, //autohome, base position
    G00 = 0, //rapid movement, move to position at max speed
    G01 = 1 //Move to position with feed rate
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
                    default: cmd->command = -1;break;
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


int main()
{
    GcodeCommand cmd;
    char line[128];
    char* filepath = "test.gcode";
    FILE* fp = fopen(filepath, "r");
    if(!fp)
    {
        perror("File can't be opened");
        return 1;
    }
    while(fgets(line, sizeof(line), fp))
    {
        if(line[0] == ';' || line[0] == '\n') continue;
        parser(line,&cmd);

        printf("parsed: G%02d", cmd.command);
        if(cmd.has_x) printf(" x=%d ",cmd.x);
        if(cmd.has_y) printf(" y=%d ",cmd.y);
        if(cmd.has_z) printf(" z=%d ",cmd.z); //probably not needed
        if(cmd.has_f) printf(" f=%d ", cmd.feedrate); //probably not needed
        printf("\n");
    }
    fclose(fp);
    return 0;

}


