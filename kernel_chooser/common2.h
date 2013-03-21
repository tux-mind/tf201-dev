// enable various options such as debug statements and extra pauses.
#define DEVELOPMENT

/* taken from http://www.termsys.demon.co.uk/vtansi.htm */
#define VT_RESET			0
#define VT_BRIGHT			1
#define VT_DIM				2
#define VT_UNDERSCORE	4
#define VT_BLINK			5
#define VT_REVERSE		7
#define VT_HIDDEN			8

#define FG_BLACK		30
#define FG_RED			31
#define FG_GREEN		32
#define FG_YELLOW		33
#define FG_BLUE			34
#define FG_MAGENTA	35
#define FG_CYAN			36
#define FG_WHITE		37

#define BG_BLACK		40
#define BG_RED			41
#define BG_GREEN		42
#define BG_YELLOW		43
#define BG_BLUE			44
#define BG_MAGENTA	45
#define BG_CYAN			46
#define BG_WHITE		47

// print helpers
#define FATAL(x,args...)	do{nc_push_message("[FATAL] "x,##args);printed_lines++;fatal_error=1;}while(0)
#define ERROR(x,args...) 	do{nc_push_message("[ERROR] "x,##args);printed_lines++;}while(0)
#define WARN(x,args...)		do{nc_push_message("[WARN ] "x,##args);printed_lines++;}while(0)
#define INFO(x,args...)		do{nc_push_message("[INFO ] "x,##args);printed_lines++;}while(0)
#ifdef DEVELOPMENT
#define DEBUG(x,args...) 	do{nc_push_message("[DEBUG] "x,##args);printed_lines++;}while(0)
#define SHELL // allow the user to drop into a shell provided by busybox
#else
#define DEBUG(x,args...)
#endif
/*
#define FATAL(x,args...)	do{snprintf(message_buffer,MAX_BUFF,"[FATAL]"x,##args);nc_push_message(message_buffer);}while(0)
#define ERROR(x,args...)	do{snprintf(message_buffer,MAX_BUFF,"[ERROR]"x,##args);nc_push_message(message_buffer);}while(0)
#define WARN(x,args...)		do{snprintf(message_buffer,MAX_BUFF,"[WARN ]"x,##args);nc_push_message(message_buffer);}while(0)
#define INFO(x,args...)		do{snprintf(message_buffer,MAX_BUFF,"[INFO ]"x,##args);nc_push_message(message_buffer);}while(0)
#define DEBUG(x,args...)	do{snprintf(message_buffer,MAX_BUFF,"[DEBUG]"x,##args);nc_push_message(message_buffer);}while(0)
*/
#define MAX_LINE 255
#define COMMAND_LINE_SIZE    1024

extern int printed_lines,fatal_error,have_default;

void nc_push_message(char *,...);
