"""Database access layer for the Grazioso Salvare dashboard.

The original project placed connection and query work close to the notebook's
interface code.  This enhanced version keeps database responsibilities in one
class, validates inputs, supports server-side paging, and keeps credentials out
of source code by accepting an environment-provided connection string.
"""

from __future__ import annotations

import os
from copy import deepcopy
from urllib.parse import quote_plus

from pymongo import ASCENDING, DESCENDING, MongoClient
from pymongo.errors import PyMongoError


class DatabaseConfigurationError(ValueError):
    """Raised when the application has not been given usable connection data."""


class DatabaseOperationError(RuntimeError):
    """Raised when MongoDB rejects a requested operation."""


class AnimalShelter:
    """Provide validated CRUD and reporting operations for AAC animal records."""

    # Only these fields may be used by the dashboard's sort control.  This
    # prevents a user-supplied field name from becoming an arbitrary query.
    ALLOWED_SORT_FIELDS = {
        "animal_id",
        "animal_type",
        "breed",
        "name",
        "age_upon_outcome_in_weeks",
    }

    # These are the fields needed by the table and map.  Projection reduces
    # network traffic and avoids exposing unrelated database fields.
    DISPLAY_PROJECTION = {
        "_id": 0,
        "animal_id": 1,
        "animal_type": 1,
        "breed": 1,
        "name": 1,
        "sex_upon_outcome": 1,
        "age_upon_outcome_in_weeks": 1,
        "location_lat": 1,
        "location_long": 1,
    }

    RESCUE_FILTERS = {
        "Reset": {},
        "Water Rescue": {
            "animal_type": "Dog",
            "breed": {
                "$regex": "Labrador Retriever|Chesapeake Bay Retriever|Newfoundland",
                "$options": "i",
            },
            "sex_upon_outcome": "Intact Female",
            "age_upon_outcome_in_weeks": {"$gte": 26, "$lte": 156},
        },
        "Mountain or Wilderness Rescue": {
            "animal_type": "Dog",
            "breed": {
                "$regex": "German Shepherd|Alaskan Malamute|Old English Sheepdog|Siberian Husky|Rottweiler",
                "$options": "i",
            },
            "sex_upon_outcome": "Intact Male",
            "age_upon_outcome_in_weeks": {"$gte": 26, "$lte": 156},
        },
        "Disaster or Individual Tracking": {
            "animal_type": "Dog",
            "breed": {
                "$regex": "Doberman Pinscher|German Shepherd|Golden Retriever|Bloodhound|Rottweiler",
                "$options": "i",
            },
            "sex_upon_outcome": "Intact Male",
            "age_upon_outcome_in_weeks": {"$gte": 20, "$lte": 300},
        },
    }

    def __init__(
        self,
        username=None,
        password=None,
        database="AAC",
        collection="animals",
        uri=None,
        client=None,
    ):
        """Configure the data layer without exposing credentials in code.

        A supplied client is useful for isolated tests.  In normal use, set
        ``AAC_MONGO_URI`` or provide credentials through environment variables.
        The client is not contacted until an operation is requested.
        """
        self.database_name = database
        self.collection_name = collection

        if client is not None:
            self.client = client
        else:
            self.client = self._build_client(username, password, uri)

        self.database = self.client[self.database_name]
        self.collection = self.database[self.collection_name]

    @classmethod
    def from_environment(cls):
        """Build a data layer from protected environment configuration."""
        return cls(
            username=os.getenv("AAC_MONGO_USERNAME"),
            password=os.getenv("AAC_MONGO_PASSWORD"),
            database=os.getenv("AAC_MONGO_DATABASE", "AAC"),
            collection=os.getenv("AAC_MONGO_COLLECTION", "animals"),
            uri=os.getenv("AAC_MONGO_URI"),
        )

    @staticmethod
    def _build_client(username, password, uri):
        configured_uri = uri or os.getenv("AAC_MONGO_URI")
        if configured_uri:
            return MongoClient(configured_uri, serverSelectionTimeoutMS=5000)

        if not username or not password:
            raise DatabaseConfigurationError(
                "Set AAC_MONGO_URI or both AAC_MONGO_USERNAME and AAC_MONGO_PASSWORD."
            )

        safe_username = quote_plus(str(username))
        safe_password = quote_plus(str(password))
        local_uri = (
            f"mongodb://{safe_username}:{safe_password}"
            "@localhost:27017/?authSource=admin"
        )
        return MongoClient(local_uri, serverSelectionTimeoutMS=5000)

    def ping(self):
        """Verify connectivity without returning database contents."""
        try:
            self.client.admin.command("ping")
            return True
        except PyMongoError as exc:
            raise DatabaseOperationError("MongoDB is unavailable.") from exc

    def ensure_indexes(self):
        """Create indexes used by rescue filters and common dashboard sorting."""
        try:
            self.collection.create_index(
                [("animal_type", ASCENDING), ("sex_upon_outcome", ASCENDING)]
            )
            self.collection.create_index(
                [("breed", ASCENDING), ("age_upon_outcome_in_weeks", ASCENDING)]
            )
            return True
        except PyMongoError as exc:
            raise DatabaseOperationError("Unable to create dashboard indexes.") from exc

    @staticmethod
    def _require_mapping(value, name):
        if not isinstance(value, dict):
            raise ValueError(f"{name} must be a dictionary.")

    def create(self, data):
        """Insert one non-empty animal record and return its acknowledgement."""
        self._require_mapping(data, "data")
        if not data:
            raise ValueError("data must contain at least one field.")
        try:
            return self.collection.insert_one(deepcopy(data)).acknowledged
        except PyMongoError as exc:
            raise DatabaseOperationError("The animal record could not be created.") from exc

    def read(self, query=None, projection=None, sort=None, skip=0, limit=0):
        """Read records with optional projection, sorting, and server-side paging."""
        query = {} if query is None else query
        self._require_mapping(query, "query")
        if skip < 0 or limit < 0:
            raise ValueError("skip and limit must be nonnegative.")

        projection = projection or {"_id": 0}
        try:
            cursor = self.collection.find(query, projection)
            if sort:
                cursor = cursor.sort(sort)
            if skip:
                cursor = cursor.skip(skip)
            if limit:
                cursor = cursor.limit(limit)
            return list(cursor)
        except PyMongoError as exc:
            raise DatabaseOperationError("The records could not be read.") from exc

    def update(self, query, updates):
        """Apply validated field updates and return the modified-record count."""
        self._require_mapping(query, "query")
        self._require_mapping(updates, "updates")
        if not updates:
            raise ValueError("updates must contain at least one field.")
        try:
            result = self.collection.update_many(query, {"$set": deepcopy(updates)})
            return result.modified_count
        except PyMongoError as exc:
            raise DatabaseOperationError("The records could not be updated.") from exc

    def delete(self, query):
        """Delete matching records and return the deleted-record count."""
        self._require_mapping(query, "query")
        try:
            return self.collection.delete_many(query).deleted_count
        except PyMongoError as exc:
            raise DatabaseOperationError("The records could not be deleted.") from exc

    @classmethod
    def rescue_filter(cls, category):
        """Return a copy of an approved rescue filter, never caller-owned state."""
        if category not in cls.RESCUE_FILTERS:
            raise ValueError(f"Unknown rescue category: {category}")
        return deepcopy(cls.RESCUE_FILTERS[category])

    def read_page(self, category="Reset", page=1, page_size=10, sort_field="animal_id"):
        """Return one bounded page and count for a dashboard request."""
        if page < 1 or page_size < 1 or page_size > 100:
            raise ValueError("page must be positive and page_size must be between 1 and 100.")
        if sort_field not in self.ALLOWED_SORT_FIELDS:
            raise ValueError(f"Unsupported sort field: {sort_field}")

        query = self.rescue_filter(category)
        skip = (page - 1) * page_size
        try:
            total = self.collection.count_documents(query)
            records = self.read(
                query,
                projection=self.DISPLAY_PROJECTION,
                sort=[(sort_field, ASCENDING)],
                skip=skip,
                limit=page_size,
            )
            return {
                "records": records,
                "total": total,
                "page": page,
                "page_size": page_size,
            }
        except PyMongoError as exc:
            raise DatabaseOperationError("The dashboard page could not be loaded.") from exc

    def aggregate_breed_summary(self, category="Reset", limit=10):
        """Calculate a compact breed summary inside MongoDB."""
        if limit < 1 or limit > 100:
            raise ValueError("limit must be between 1 and 100.")
        pipeline = [
            {"$match": self.rescue_filter(category)},
            {"$group": {"_id": "$breed", "count": {"$sum": 1}}},
            {"$sort": {"count": -1, "_id": 1}},
            {"$limit": limit},
            {"$project": {"_id": 0, "breed": "$_id", "count": 1}},
        ]
        try:
            return list(self.collection.aggregate(pipeline))
        except PyMongoError as exc:
            raise DatabaseOperationError("The breed summary could not be calculated.") from exc
