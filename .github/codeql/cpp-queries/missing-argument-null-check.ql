/**
 * @name Pointer argument is dereferenced without checking for NULL
 * @description Functions which dereference a pointer should test for NULL.
 *              This should be done as an explicit comparison in an assert or if statement.
 * @kind problem
 * @problem.severity recommendation
 * @id cpp/missing-argument-null-check
 * @tags readability
 *       correctness
 *       safety
 * @precision very-high
 */

import cpp

predicate hasNullCheck(Function func, Parameter p){
  exists(MacroInvocation m |
    m.getMacroName() = "assert"
    and m.getEnclosingFunction() = func
    and m.getUnexpandedArgument(0) = p.getName() + " != NULL")
  or
  exists(EqualityOperation comparison, MacroInvocation m|
    comparison.getEnclosingFunction() = func
    and comparison.getLeftOperand().toString() = p.getName()
    and comparison.getRightOperand() = m.getExpr()
    and m.getMacroName() = "NULL")
}

from Function func, PointerFieldAccess acc, Parameter p, PointerType pt
where acc.getEnclosingFunction() = func
  and p.getFunction() = func
  and p.getType() = pt
  and acc.getQualifier().toString() = p.getName()
  and not hasNullCheck(func, p)
select acc, "Parameter " + p.getName() +
            " in " + func.getName() +
            "() is dereferenced without an explicit null-check"
