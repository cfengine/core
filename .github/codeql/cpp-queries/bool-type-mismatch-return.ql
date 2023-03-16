/**
 * @name Boolean type mismatch between function type and return value
 * @description Strict boolean type-checking for return statements.
 *              Boolean functions must return something boolean.
 *              Non-boolean functions must not return something boolean.
 *              Comparisons, logical AND/OR, and true/false macros are bool.
 * @kind problem
 * @problem.severity warning
 * @id cpp/bool-type-mismatch-return
 * @tags readability
 *       correctness
 * @precision very-high
 */

import cpp

predicate isBoolExpr(Expr expression){
  // Not bool if there is an explicit cast to something else:
  not exists(CStyleCast cast |
    cast.getType().getName() != "bool"
    and
    cast.getExplicitlyConverted() = expression.getExplicitlyConverted()
  )
  and
  // One of these imply boolean:
  (
    // variable of type bool:
    expression.getType().getName() = "bool"
    // true or false macro:
    or exists(MacroInvocation m |
           m.getExpr() = expression
           and (m.getMacroName() = "true" or m.getMacroName() = "false"))
    // && or || operator
    or exists(BinaryLogicalOperation b |
              b = expression)
    // == or != or > or < or >= or <= operator:
    or exists(ComparisonOperation cmp |
              cmp = expression)
    // ! operator:
    or exists(NotExpr n |
              n = expression
              and isBoolExpr(n.getOperand()))
    // Recursively check both branches of ternary operator:
    or exists(ConditionalExpr c |
              c = expression
              and isBoolExpr(c.getThen())
              and isBoolExpr(c.getElse()))
  )
}

string showMacroExpr(Expr e){
  not e.isInMacroExpansion()
  and result = e.toString()
or
  e.isInMacroExpansion()                     // Show true=1/false=0 in alert
  and result = e.findRootCause()
                  .toString()
                  .replaceAll("#define ", "")
                  .replaceAll(" ", "=")
}

from Function f, ReturnStmt r, string rt
where r.getEnclosingFunction() = f
  and (
      f.getType().getName() = "bool"
      and not isBoolExpr(r.getExpr())
      and rt = "non-bool"
    or
      f.getType().getName() != "bool"
      and isBoolExpr(r.getExpr())
      and rt = "bool"
  )
select r, "Function " + f.getName() +
          " has return type " + f.getType().getName() +
          " and returns " + rt +
          "(" + showMacroExpr(r.getExpr()) + ")"
