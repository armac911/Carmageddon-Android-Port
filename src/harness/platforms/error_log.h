#ifndef DR_ERROR_LOG_H
#define DR_ERROR_LOG_H
void DRErrorLogStart(void);
void DRErrorLogStop(void);
void DRErrorLogMessage(int priority, const char* prefix, const char* body);
#endif
