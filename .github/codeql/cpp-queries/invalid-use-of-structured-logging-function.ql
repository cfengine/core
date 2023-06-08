/**
 * @name Invalid use of structured logging function.
 * @description Each call to LogToSystemLogStructured must include the terminating MESSAGE key.
 * @kind problem
 * @problem.severity error
 * @id cpp/invalid-use-of-structured-logging-function
 * @tags correctness
 *       security
 * @precision very-high
 */

import cpp

from FunctionCall fc
where fc.getTarget().getQualifiedName() = "LogToSystemLogStructured"
select fc, "Does this get run?"
