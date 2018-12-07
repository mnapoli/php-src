/*
   +----------------------------------------------------------------------+
   | PHP Version 7                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2018 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Matthieu Napoli <matthieu@mnapoli.fr>                        |
   +----------------------------------------------------------------------+
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

#ifdef PHP_WIN32
# include <process.h>
# include <io.h>
# include "win32/time.h"
# include "win32/signal.h"
# include "win32/php_registry.h"
# include <sys/timeb.h>
#else
# include "php_config.h"
#endif

#ifdef __riscos__
#include <unixlib/local.h>
#endif

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if HAVE_SETLOCALE
#include <locale.h>
#endif
#if HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include "SAPI.h"
#include "php.h"
#include "php_ini.h"
#include "php_main.h"
#include "php_globals.h"
#include "php_variables.h"
#include "zend_hash.h"
#include "zend_modules.h"
#include "fopen_wrappers.h"
#include "cli.h"

#include "zend_compile.h"
#include "zend_execute.h"
#include "zend_highlight.h"
#include "zend_exceptions.h"

#include "php_getopt.h"

#ifndef PHP_WIN32
# define php_select(m, r, w, e, t)	select(m, r, w, e, t)
# define SOCK_EINVAL EINVAL
# define SOCK_EAGAIN EAGAIN
# define SOCK_EINTR EINTR
# define SOCK_EADDRINUSE EADDRINUSE
#else
# include "win32/select.h"
# define SOCK_EINVAL WSAEINVAL
# define SOCK_EAGAIN WSAEWOULDBLOCK
# define SOCK_EINTR WSAEINTR
# define SOCK_EADDRINUSE WSAEADDRINUSE
#endif

#include "ext/standard/file.h" /* for php_set_sock_blocking() :-( */
#include "zend_smart_str.h"
#include "ext/standard/html.h"
#include "ext/standard/url.h" /* for php_raw_url_decode() */
#include "ext/standard/php_string.h" /* for php_dirname() */
#include "ext/date/php_date.h" /* for php_format_date() */

#include "php_cli_loop.h"

ZEND_DECLARE_MODULE_GLOBALS(cli_loop);

PHP_INI_BEGIN()
    // No ini settings to define
PHP_INI_END()

static PHP_MINIT_FUNCTION(cli_loop)
{
    REGISTER_INI_ENTRIES();
    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(cli_loop)
{
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

static PHP_MINFO_FUNCTION(cli_loop)
{
    DISPLAY_INI_ENTRIES();
}

zend_module_entry cli_loop_module_entry = {
    STANDARD_MODULE_HEADER,
    "cli_loop",
    NULL,
    PHP_MINIT(cli_loop),
    PHP_MSHUTDOWN(cli_loop),
    NULL,
    NULL,
    PHP_MINFO(cli_loop),
    PHP_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

static int sapi_cli_loop_startup(sapi_module_struct *sapi_module) /* {{{ */
{
    if (php_module_startup(sapi_module, &cli_loop_module_entry, 1) == FAILURE) {
        return FAILURE;
    }
    return SUCCESS;
} /* }}} */

static size_t sapi_cli_loop_ub_write(const char *str, size_t str_length) /* {{{ */
{
    const char *ptr = str;
    size_t remaining = str_length;
    ssize_t ret;

    if (!str_length) {
        return 0;
    }

    while (remaining > 0)
    {
        ret = sapi_cli_single_write(ptr, remaining);
        if (ret < 0) {
#ifndef PHP_CLI_WIN32_NO_CONSOLE
            EG(exit_status) = 255;
            php_handle_aborted_connection();
#endif
            break;
        }
        ptr += ret;
        remaining -= ret;
    }

    return (ptr - str);
}
/* }}} */

static void sapi_cli_loop_flush(void *server_context) /* {{{ */
{
    /* Ignore EBADF here, it's caused by the fact that STDIN/STDOUT/STDERR streams
     * are/could be closed before fflush() is called.
     */
    if (fflush(stdout)==EOF && errno!=EBADF) {
#ifndef PHP_CLI_WIN32_NO_CONSOLE
        php_handle_aborted_connection();
#endif
    }
}
/* }}} */

static int sapi_cli_loop_send_headers(sapi_headers_struct *sapi_headers) /* {{{ */
{
    /* We do nothing here, this function is needed to prevent that the fallback
     * header handling is called. */
    return SAPI_HEADER_SENT_SUCCESSFULLY;
}
/* }}} */

static void sapi_cli_loop_log_message(char *msg, int syslog_type_int) /* {{{ */
{
    // Log to stderr
    fprintf(stderr, "%s\n", msg);
} /* }}} */

/* {{{ sapi_module_struct cli_loop_sapi_module
 */
sapi_module_struct cli_loop_sapi_module = {
		"cli-loop",						/* name */
		"Loop runner",					/* pretty name */

		sapi_cli_loop_startup,			/* startup */
		php_module_shutdown_wrapper,	/* shutdown */

		NULL,							/* activate */
		NULL,							/* deactivate */

        sapi_cli_loop_ub_write,		    /* unbuffered write */
        sapi_cli_loop_flush,			/* flush */
		NULL,							/* get uid */
		NULL,							/* getenv */

		php_error,						/* error handler */

		NULL,							/* header handler */
        sapi_cli_loop_send_headers,		/* send headers handler */
		NULL,							/* send header handler */

		NULL,							/* read POST data */
		NULL,							/* read Cookies */

		NULL,							/* register server variables */
		sapi_cli_loop_log_message,		/* Log message */
		NULL,							/* Get request time */
		NULL,							/* Child terminate */

		STANDARD_SAPI_MODULE_PROPERTIES
}; /* }}} */

static int php_cli_loop_run_script(const char *filename) /* {{{ */
{
	int stop = 0;
	zend_file_handle zfd;
	char *old_cwd;

	ALLOCA_FLAG(use_heap)
	old_cwd = do_alloca(MAXPATHLEN, use_heap);
	old_cwd[0] = '\0';
	php_ignore_value(VCWD_GETCWD(old_cwd, MAXPATHLEN - 1));

	zfd.type = ZEND_HANDLE_FILENAME;
	zfd.filename = filename;
	zfd.handle.fp = NULL;
	zfd.free_filename = 0;
	zfd.opened_path = NULL;

	zend_try {
        zval retval;

        ZVAL_UNDEF(&retval);
        if (SUCCESS == zend_execute_scripts(ZEND_REQUIRE, &retval, 1, &zfd)) {
            if (Z_TYPE(retval) != IS_UNDEF) {
                // If it's `false` then stop
                stop = Z_TYPE(retval) == IS_FALSE;
                zval_ptr_dtor(&retval);
            }
        } else {
            stop = 0;
        }
	} zend_end_try();

	if (old_cwd[0] != '\0') {
		php_ignore_value(VCWD_CHDIR(old_cwd));
	}

	free_alloca(old_cwd, use_heap);

	return stop;
}
/* }}} */

static int php_cli_loop_is_running = 1;

static void php_cli_loop_sigint_handler(int sig) /* {{{ */
{
	php_cli_loop_is_running = 0;
}
/* }}} */

int do_cli_loop(int argc, char **argv) /* {{{ */
{
	extern const opt_struct OPTIONS[];
	char *php_optarg = NULL;
	int php_optind = 1;
	int c;
    int stop = 0;
	const char *script_name = NULL;

	// Read the script name from the `-L` option
	while ((c = php_getopt(argc, argv, OPTIONS, &php_optarg, &php_optind, 0, 2))!=-1) {
		switch (c) {
			case 'L':
				script_name = php_optarg;
				break;
		}
	}
    if (!script_name) {
        php_printf("You must provide a script to execute\n");
        return 1;
    }

#if defined(HAVE_SIGNAL_H) && defined(SIGINT)
	signal(SIGINT, php_cli_loop_sigint_handler);
	zend_signal_init();
#endif

	while (php_cli_loop_is_running) {
		if (FAILURE == php_request_startup()) {
			return FAILURE;
		}

        stop = php_cli_loop_run_script(script_name);

        php_request_shutdown(0);

		if (stop) {
			return FAILURE;
		}
	}

	return 0;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
