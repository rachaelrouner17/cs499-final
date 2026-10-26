"""Service layer used by the enhanced animal-rescue dashboard.

The service keeps dashboard-facing operations separate from MongoDB details.
It delegates validation, filtering, pagination, and aggregation to
``AnimalShelter`` while returning simple dictionaries that a notebook or web
view can render.
"""

from CRUD_Python_Module import AnimalShelter


class DashboardDataService:
    """Coordinate dashboard requests without exposing database mechanics."""

    def __init__(self, repository=None):
        self.repository = repository or AnimalShelter.from_environment()

    def initialize(self):
        """Verify the connection and create indexes needed by the dashboard."""
        self.repository.ping()
        self.repository.ensure_indexes()
        return True

    def page(self, category="Reset", page=1, page_size=10, sort_field="animal_id"):
        """Return one bounded table page for an approved rescue category."""
        return self.repository.read_page(
            category=category,
            page=page,
            page_size=page_size,
            sort_field=sort_field,
        )

    def breed_summary(self, category="Reset", limit=10):
        """Return the most common breeds for the selected category."""
        return self.repository.aggregate_breed_summary(category=category, limit=limit)

