"""Offline tests for the enhanced database layer.

These tests use an in-memory collection double. They verify application logic
without claiming that the unavailable original AAC MongoDB service is online.
"""

import unittest

from CRUD_Python_Module import AnimalShelter
from DashboardDataService import DashboardDataService


class FakeCursor:
    def __init__(self, records):
        self.records = list(records)

    def sort(self, fields):
        for field, direction in reversed(fields):
            self.records.sort(key=lambda item: item.get(field, ""), reverse=direction < 0)
        return self

    def skip(self, amount):
        self.records = self.records[amount:]
        return self

    def limit(self, amount):
        self.records = self.records[:amount]
        return self

    def __iter__(self):
        return iter(self.records)


class FakeCollection:
    def __init__(self):
        self.records = [
            {"animal_id": "A1", "animal_type": "Dog", "breed": "Labrador Retriever"},
            {"animal_id": "A2", "animal_type": "Cat", "breed": "Domestic Shorthair"},
        ]
        self.indexes = []

    def create_index(self, fields):
        self.indexes.append(fields)
        return "index"

    def find(self, query, projection):
        records = self.records
        if query.get("animal_type"):
            records = [r for r in records if r.get("animal_type") == query["animal_type"]]
        output = []
        for record in records:
            output.append({key: value for key, value in record.items() if projection.get(key, 0)})
        return FakeCursor(output)

    def count_documents(self, query):
        return len(self.records)

    def insert_one(self, record):
        self.records.append(record)
        return type("Result", (), {"acknowledged": True})()

    def update_many(self, query, update):
        changed = 0
        for record in self.records:
            if all(record.get(key) == value for key, value in query.items()):
                record.update(update["$set"])
                changed += 1
        return type("Result", (), {"modified_count": changed})()

    def delete_many(self, query):
        before = len(self.records)
        self.records = [
            record for record in self.records
            if not all(record.get(key) == value for key, value in query.items())
        ]
        return type("Result", (), {"deleted_count": before - len(self.records)})()

    def aggregate(self, pipeline):
        return [{"breed": "Labrador Retriever", "count": 1}]


class FakeDatabase:
    def __init__(self, collection):
        self.collection = collection

    def __getitem__(self, name):
        return self.collection


class FakeAdmin:
    def command(self, name):
        return {"ok": 1}


class FakeClient:
    def __init__(self, collection):
        self.database = FakeDatabase(collection)
        self.admin = FakeAdmin()

    def __getitem__(self, name):
        return self.database


class DatabaseLayerTests(unittest.TestCase):
    def setUp(self):
        collection = FakeCollection()
        self.repository = AnimalShelter(client=FakeClient(collection))
        self.service = DashboardDataService(self.repository)

    def test_crud_operations(self):
        self.assertTrue(self.repository.create({"animal_id": "A3", "breed": "Beagle"}))
        self.assertEqual(self.repository.update({"animal_id": "A3"}, {"name": "Scout"}), 1)
        self.assertEqual(self.repository.delete({"animal_id": "A3"}), 1)

    def test_page_and_summary_are_delegated(self):
        page = self.service.page(page=1, page_size=1)
        self.assertEqual(page["total"], 2)
        self.assertEqual(len(page["records"]), 1)
        self.assertEqual(self.service.breed_summary()[0]["breed"], "Labrador Retriever")

    def test_allowlist_rejects_invalid_inputs(self):
        with self.assertRaises(ValueError):
            self.repository.read_page(sort_field="password")
        with self.assertRaises(ValueError):
            self.repository.rescue_filter("Unknown category")

    def test_initialization_checks_connection_and_indexes(self):
        self.assertTrue(self.service.initialize())


if __name__ == "__main__":
    unittest.main()
