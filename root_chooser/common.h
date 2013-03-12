// enable various options such as debug statements and extra pauses.
#define DEVELOPMENT

// print helpers
#define FATAL(x,args...)	do{printf("[FATAL] "x,##args);printed_lines++;fatal_error=1;}while(0)
#define ERROR(x,args...) 	do{printf("[ERROR] "x,##args);printed_lines++;}while(0)
#define WARN(x,args...)		do{printf("[WARN ] "x,##args);printed_lines++;}while(0)
#define INFO(x,args...)		do{printf("[INFO ] "x,##args);printed_lines++;}while(0)
#ifdef DEVELOPMENT
#define DEBUG(x,args...) 	do{printf("[DEBUG] "x,##args);printed_lines++;}while(0)
#define STOP_BEFORE_MENU
#define SHELL // allow the user to drop into a shell provided by busybox
#else
#define DEBUG(x,args...)
#endif


#define MAX_LINE 255
#define COMMAND_LINE_SIZE    1024

extern int printed_lines,fatal_error,have_default;
