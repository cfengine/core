/**
 * @name Invaldig use of structured logging function.
 * @description Each call to LogToSystemLogStructured must include the terminating "MESSAGE" key.
 * @kind problem
 * @problem.severity error
 * @id cpp/invalid-use-of-structured-logging-function
 * @tags correctness
 *       safety
 * @precision very-high
 */

import cpp

select "Does this get run?"
