/**
 * @name Bool function returns something not boolean
 * @description Functions with return type 'bool' should return something boolean.
 *              This can for example be a comparison, a 'bool' variable, or a
 *              'true'/'false' macro.
 * @kind problem
 * @problem.severity error
 * @id cpp/bool-function-returns-non-bool
 * @tags readability
 *       correctness
 * @precision very-high
 */

import cpp

// TODO: This query can be improved to catch more edge cases:
//       A recursive isBool predicate could be used inside
//       NotExpr and ConditionalExpr to better determine the
//       return type.

from Function f, ReturnStmt r
where f.getType().getName() = "bool"                  // Function return type is bool
  and r.getEnclosingFunction() = f                    // Return statement is inside function
  and r.getExpr().getType().toString() != "bool"      // Return expression is non-bool
  and not exists(MacroInvocation m |                  // Ignore true/false macro
                 m.getExpr() = r.getExpr()
                 and (m.getMacroName() = "true"
                      or m.getMacroName() = "false"))
  and not exists(BinaryLogicalOperation b |           // Ignore && or || expression
                 b = r.getExpr())
  and not exists(ComparisonOperation cmp |            // Ignore == or != expression
                 cmp = r.getExpr())
  and not exists(NotExpr n | n = r.getExpr())         // Ignore !() expression
  and not exists(ConditionalExpr c | c = r.getExpr()) // Ignore ()?():() expression
select r, "Function " + f.getName() +
          " has return type " + f.getType().getName() +
          " and returns " + r.getExpr().getType().toString() +
      "(" + r.getExpr().toString() + ")"
