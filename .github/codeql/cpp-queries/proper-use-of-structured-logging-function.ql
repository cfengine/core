import cpp

from FunctionCall fc
where fc.getTarget().getQualifiedName() = "Log"
select fc, "Does this get run?"
