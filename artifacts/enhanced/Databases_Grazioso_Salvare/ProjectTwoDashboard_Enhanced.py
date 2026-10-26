"""Dashboard-facing adapter for the database-enhanced animal application.

The original notebook remains available for comparison. This module shows the
intended integration point: a presentation layer requests bounded pages and
summaries from DashboardDataService instead of embedding credentials or raw
MongoDB queries in callback code.
"""

from DashboardDataService import DashboardDataService


def load_dashboard_page(service=None, category="Reset", page=1, page_size=10):
    """Load one table page using approved filters and server-side paging."""
    service = service or DashboardDataService()
    return service.page(category=category, page=page, page_size=page_size)


def load_breed_summary(service=None, category="Reset", limit=10):
    """Load chart data from the database aggregation pipeline."""
    service = service or DashboardDataService()
    return service.breed_summary(category=category, limit=limit)


def initialize_dashboard(service=None):
    """Prepare indexes and verify connectivity for a real deployment."""
    service = service or DashboardDataService()
    return service.initialize()


if __name__ == "__main__":
    # A deployment supplies AAC_MONGO_URI or credential environment variables.
    # No credentials are embedded in this module.
    service = DashboardDataService()
    initialize_dashboard(service)
    print(load_dashboard_page(service, page=1, page_size=10))
