#ifndef SETTINGS_H
#define SETTINGS_H

#define CONTAINER_CNT   1   ///< maximum number of containers

#define PAGE_SIZE       4096

#define CACHE_LINE_SIZE 64

/* file mapper settings */
#define FM_FILE_SIZE    (PAGE_SIZE * 2)

#define FM_FILE_NAME_PREFIX "/tmp/container"

#define PMLIB_LOG_PREFIX    "[PMLIB]"
#define PMLIB_LOG_LEVEL_VAR "PMLIB_LOG_LEVEL"
#define PMLIB_LOG_FILE_VAR  "PMLIB_LOG_FILE"
#define PMLIB_MAJOR_VERSION 0
#define PMLIB_MINOR_VERSION 1

// The following line enables the loggin system
#define OUT_ENABLE

// The following line enables stats
#define STATS_ENABLED

#endif /* end of include guard: SETTINGS_H */
