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

from Function func, PointerFieldAccess acc, Parameter p, PointerType pt
where acc.getEnclosingFunction() = func
  and p.getFunction() = func
  and p.getType() = pt
  and acc.getQualifier().toString() = p.getName()
  and not
  (
    exists(EqualityOperation comparison |
           comparison.getEnclosingFunction() = func
           and comparison.getLeftOperand().toString() = p.getName()
           and comparison.getRightOperand().findRootCause().toString() = "NULL")
  )
select acc, "Parameter " + p.getName() +
            " in " + func.getName() +
            "() is dereferenced without a null-check"
