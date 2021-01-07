#python unit test
import unittest
import os
import sys
from tests import mock

import pytest
import json
os.environ['UNITTEST'] = "1"
sys.path.insert(0, "../../pybind/mgr")
from pg_autoscaler import module

class TestPgAutoscaler(object):

    def setup(self):
        # a bunch of attributes for testing
        self.test_event = module.PgAutoscaler('module_name', 0, 0)
        self.test_event._full_pg = None
        self.test_event._pool_count = None
        self.test_event._pgs_left = None

    def test_sample(self):
        assert 3+1 == 4
    
    def test_get_pool_status(self):
        threshold = 3.0
        pools = {

                "test0":{
                    
                    "pool": 0,
                    "pool_name": "test0",
                    "pg_num_target": 32,
                    "capacity_ratio": 0.2
                    },

                "test1":{

                    "pool": 1,
                    "pool_name": "test1",
                    "pg_num_target": 32,
                    "capacity_ratio": 0.2
                    },

                "test2":{

                    "pool": 2,
                    "pool_name": "test2",
                    "pg_num_target": 32,
                    "capacity_ratio": 0.2
                    },

                "test3":{

                    "pool": 3,
                    "pool_name": "test3",
                    "pg_num_target": 32,
                    "capacity_ratio": 0.1
                    },

                }

        self.test_event._pool_count = len(pools.items())
        self.test_event._full_pg = 128
        self.test_event._pgs_left = self.test_event._full_pg

        for pool_name, p in pools.items():
            final_pg_target, final_ratio, pool_pg_target = self.test_event._get_final_pg_target_and_ratio(
                                                                        p['capacity_ratio'])
            adjust = False
            if (final_pg_target > p['pg_num_target'] * threshold or \
                final_pg_target < p['pg_num_target'] / threshold) and \
                final_ratio >= 0.0 and \
                final_ratio <= 1.0:
                adjust = True
            
            assert adjust == False

        assert self.test_event._pgs_left == 0
