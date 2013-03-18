// enable various options such as debug statements and extra pauses.
#define DEVELOPMENT

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
#define FATAL(x,args...)	do{printf("\033[%d;%dm[FATAL]\033[0m "x,BG_BLACK,FG_RED,##args);printed_lines++;fatal_error=1;}while(0)
#define ERROR(x,args...) 	do{printf("\033[%d;%dm[ERROR]\033[0m "x,BG_BLACK,FG_RED,##args);printed_lines++;}while(0)
#define WARN(x,args...)		do{printf("\033[%d;%dm[WARN ]\033[0m "x,BG_BLACK,FG_RED,##args);printed_lines++;}while(0)
#define INFO(x,args...)		do{printf("\033[%d;%dm[INFO ]\033[0m "x,BG_BLACK,FG_RED,##args);printed_lines++;}while(0)
#ifdef DEVELOPMENT
#define DEBUG(x,args...) 	do{printf("\033[%d;%dm[DEBUG]\033[0m "x,BG_BLACK,FG_RED,##args);printed_lines++;}while(0)
#define SHELL // allow the user to drop into a shell provided by busybox
#else
#define DEBUG(x,args...)
#endif


#define MAX_LINE 255
#define COMMAND_LINE_SIZE    1024

extern int printed_lines,fatal_error,have_default;
