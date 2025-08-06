from cfengine import (
    PromiseModule,
    Result,
)


class DummyPromiseType(PromiseModule):

    def __init__(self, **kwargs):
        super(DummyPromiseType, self).__init__(
            name="dummy_promise_module", version="0.0.0", **kwargs
        )

    def validate_promise(self, promiser, attributes, meta):
        pass

    def evaluate_promise(self, promiser, attributes, metadata):

        with open(promiser, "a") as f:
            f.write("test")

        return Result.KEPT


if __name__ == "__main__":
    DummyPromiseType().start()
