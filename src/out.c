/*
 * Copyright (c) 2014-2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * out.c -- support for logging, tracing, and assertion output
 *
 * Macros like LOG(), OUT, ASSERT(), etc. end up here.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

//#include "libvmem.h"

#include "out.h"

char nvml_src_version[] = "SRCVERSION:" ;

static const char *Log_prefix;
static int Log_level;
static FILE *Out_fp;

#ifdef	OUT_ENABLE
/*
 * getexecname -- return name of current executable
 *
 * This function is only used when logging is enabled, to make
 * it more clear in the log which program was running.
 */
static const char *
getexecname(void)
{
	char procpath[PATH_MAX];
	static char namepath[PATH_MAX];
	int cc;

	snprintf(procpath, PATH_MAX, "/proc/%d/exe", getpid());

	if ((cc = readlink(procpath, namepath, PATH_MAX)) < 0)
		strcpy(namepath, "unknown");
	else
		namepath[cc] = '\0';

	return namepath;
}
#endif	/* OUT_ENABLE */

/*
 * out_init -- initialize the log
 *
 * This is called from the library initialization code.
 */
void
out_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var, int major_version,
		int minor_version)
{
	static int once;

	/* only need to initialize the out module once */
	if (once)
		return;
	once++;

	Log_prefix = log_prefix;

#ifdef	OUT_ENABLE
    fprintf(stdout, "[PMLib] WARNING: Logging is enabled!\n");
	char *log_level;
	char *log_file;

	if ((log_level = getenv(log_level_var)) != NULL) {
		Log_level = atoi(log_level);
		if (Log_level < 0) {
			Log_level = 0;
		}
	}

	if ((log_file = getenv(log_file_var)) != NULL) {
		int cc = strlen(log_file);

		/* reserve more than enough space for a PID + '\0' */
		char log_file_pid[cc + 30];

		if (cc > 0 && log_file[cc - 1] == '-') {
			snprintf(log_file_pid, cc + 30, "%s%d",
				log_file, getpid());
			log_file = log_file_pid;
		}
		if ((Out_fp = fopen(log_file, "w")) == NULL) {
			fprintf(stderr, "Error (%s): %s=%s: %s\n",
					log_prefix, log_file_var,
					log_file, strerror(errno));
			exit(1);
		}
	}
#endif	/* OUT_ENABLE */

	if (Out_fp == NULL)
		Out_fp = stderr;
	else
		setlinebuf(Out_fp);

	LOG(1, "pid %d: program: %s", getpid(), getexecname());
	LOG(1, "%s version %d.%d", log_prefix, major_version, minor_version);
	LOG(1, "src version %s", nvml_src_version);
#ifdef USE_VALGRIND
	LOG(1, "compiled with support for Valgrind");
#endif /* USE_VALGRIND */
}

/*
 * out_fini -- close the log file
 *
 * This is called to close log file before process stop.
 */
void
out_fini()
{
	if (Out_fp != NULL && Out_fp != stderr) {
		fclose(Out_fp);
		Out_fp = stderr;
	}
}

/*
 * out_print_func -- default print_func, goes to stderr or Out_fp
 */
static void
out_print_func(const char *s)
{
	fputs(s, Out_fp);
}

/*
 * calling Print(s) calls the current print_func...
 */
typedef void (*Print_func)(const char *s);
typedef int (*Vsnprintf_func)(char *str, size_t size, const char *format,
		va_list ap);
static Print_func Print = out_print_func;
static Vsnprintf_func Vsnprintf = vsnprintf;

/*
 * out_set_print_func -- allow override of print_func used by out module
 */
void
out_set_print_func(void (*print_func)(const char *s))
{
	LOG(3, "print %p", print_func);

	Print = (print_func == NULL) ? out_print_func : print_func;
}

/*
 * out_set_vsnprintf_func -- allow override of vsnprintf_func used by out module
 */
void
out_set_vsnprintf_func(int (*vsnprintf_func)(char *str, size_t size,
				const char *format, va_list ap))
{
	LOG(3, "vsnprintf %p", vsnprintf_func);

	Vsnprintf = (vsnprintf_func == NULL) ? vsnprintf : vsnprintf_func;
}

/*
 * out_snprintf -- (internal) custom snprintf implementation
 */
__attribute__((format(printf, 3, 4)))
static int
out_snprintf(char *str, size_t size, const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = Vsnprintf(str, size, format, ap);
	va_end(ap);

	return (ret);
}

/*
 * out_common -- common output code, all output goes through here
 */
#define	MAXPRINT 8192	/* maximum expected log line */
static void
out_common(const char *file, int line, const char *func, int level,
		const char *suffix, const char *fmt, va_list ap)
{
	int oerrno = errno;
	char buf[MAXPRINT];
	int cc = 0;
	const char *sep = "";
	const char *errstr = "";

	if (file)
		cc += out_snprintf(&buf[cc], MAXPRINT - cc,
				"<%s>: <%d> [%s:%d %s] ",
				Log_prefix, level, file, line, func);

	if (fmt) {
		if (*fmt == '!') {
			fmt++;
			sep = ": ";
			errstr = strerror(errno);
		}
		cc += Vsnprintf(&buf[cc], MAXPRINT - cc, fmt, ap);
	}

	out_snprintf(&buf[cc], MAXPRINT - cc, "%s%s%s", sep, errstr, suffix);

	Print(buf);
	errno = oerrno;
}

/*
 * out -- output a line, newline added automatically
 */
void
out(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	out_common(NULL, 0, NULL, 0, "\n", fmt, ap);

	va_end(ap);
}

/*
 * out_nonl -- output a line, no newline added automatically
 */
void
out_nonl(int level, const char *fmt, ...)
{
	va_list ap;

	if (Log_level < level)
			return;

	va_start(ap, fmt);
	out_common(NULL, 0, NULL, level, "", fmt, ap);

	va_end(ap);
}

/*
 * out_log -- output a log line if Log_level >= level
 */
void
out_log(const char *file, int line, const char *func, int level,
		const char *fmt, ...)
{
	va_list ap;

	if (Log_level < level)
			return;

	va_start(ap, fmt);
	out_common(file, line, func, level, "\n", fmt, ap);

	va_end(ap);
}

/*
 * out_fatal -- output a fatal error & die (i.e. assertion failure)
 */
void
out_fatal(const char *file, int line, const char *func,
		const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	out_common(file, line, func, 1, "\n", fmt, ap);

	va_end(ap);

	exit(1);
}
