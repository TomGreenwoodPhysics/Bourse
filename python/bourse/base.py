"""Agent interface.  [PHASE 4]"""

from abc import ABC, abstractmethod


class Agent(ABC):
    """Base class for all trading agents.

    An agent observes book state each step and returns a list of actions
    (new limit orders, cancels). Concrete subclasses implement `on_step`.
    """

    def __init__(self, name: str):
        self.name = name

    @abstractmethod
    def on_step(self, book, t):
        """Observe the book at logical time `t`; return a list of actions."""
        raise NotImplementedError