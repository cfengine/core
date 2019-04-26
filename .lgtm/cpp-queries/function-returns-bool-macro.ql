/**
 * @name Non-bool function returns true or false macro
 * @description Functions with return type other than 'bool' should not return 'true' or 'false'.
 *              'true' and 'false' macros are technically 'int', but we want type checking.
 * @kind problem
 * @problem.severity error
 * @id cpp/function-returns-bool-macro
 * @tags readability
 *       correctness
 * @precision very-high
 */

import cpp

from Function f, ReturnStmt r, MacroInvocation m // Select all functions, return statements and macro invocations
where r.getEnclosingFunction() = f               // Return statement is inside function
  and r.hasExpr()                                // Return statement returns something (not return;)
  and f.getType().getName() != "bool"            // Function has non-bool return type
  and f.getType().getName() != "int"             // Ignore int functions as well, for now (old code has a lot)
  and m.getExpr() = r.getExpr()                  // The returned value (expression) is a macro
  and (m.getMacroName() = "true"                 // Macro is "true"
       or m.getMacroName() = "false")            // or "false"
select r, "Function " + f.getName() +            // Select the return statement as the alert, and print a nice message
          " has return type " + f.getType().getName() +
          " and returns bool (" + m.getMacroName() + ")"
