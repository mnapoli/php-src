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

#ifndef PHP_CLI_LOOP_H
#define PHP_CLI_LOOP_H

#include "SAPI.h"

extern sapi_module_struct cli_loop_sapi_module;
extern int do_cli_loop(int argc, char **argv);

ZEND_BEGIN_MODULE_GLOBALS(cli_loop)
    // none
ZEND_END_MODULE_GLOBALS(cli_loop)

#ifdef ZTS
#define CLI_LOOP_G(v) ZEND_TSRMG(cli_loop_globals_id, zend_cli_loop_globals *, v)
ZEND_TSRMLS_CACHE_EXTERN()
#else
#define CLI_LOOP_G(v) (cli_loop_globals.v)
#endif

#endif /* PHP_CLI_LOOP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
