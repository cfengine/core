/**
 * @name Non-bool function returns bool type variable
 * @description Functions with return type other than 'bool' should not return 'bool'.
 * @kind problem
 * @problem.severity error
 * @id cpp/function-returns-bool-variable
 * @tags readability
 *       correctness
 * @precision very-high
 */

import cpp

from Function f, ReturnStmt r                    // Select all functions and return statements
where r.getEnclosingFunction() = f               // Return statement is inside function
  and r.hasExpr()                                // Return statement returns something (not return;)
  and r.getExpr().getType().toString() = "bool"  // Returns a boolean variable (or something with type bool)
  and f.getType().getName() != "bool"            // Function has non-bool return type
select r, "Function " + f.getName() +            // Select the return statement as the alert, and print a nice message
          " has return type " + f.getType().getName() +
          " and returns bool (" + r.getExpr().toString() + ")"
