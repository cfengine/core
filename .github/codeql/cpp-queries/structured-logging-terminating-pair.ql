/**
 * @name Invalid use of structured logging function.
 * @description Each call to LogToSystemLogStructured must include the terminating "MESSAGE" key.
 * @kind problem
 * @problem.severity error
 * @id cpp/structured-logging-terminating-pair
 * @tags correctness
 *       security
 * @precision very-high
 */

import cpp

from FunctionCall fc
where fc.getTarget().getQualifiedName() = "LogToSystemLogStructured"
    and fc.getArgument(_) instanceof StringLiteral
    and not fc.getArgument(_).toString() = "MESSAGE"
select fc, "LogToSystemLogStructured requires the terminating key-value pair containing the \"MESSAGE\" key"
