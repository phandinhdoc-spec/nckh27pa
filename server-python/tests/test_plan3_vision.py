import json
import pytest
from thien_nhan.plan3_vision import center_obstacles


def test_only_boxes_covering_center_trigger():
    hit = {"label": "chair", "box": [300, 200, 400, 500]}
    miss = {"label": "table", "box": [10, 10, 200, 500]}
    assert center_obstacles(json.dumps({"obstacles": [hit, miss]})) == [hit]
    assert center_obstacles('{"obstacles":[]}') == []


def test_image_edge_and_invalid_extra_box_preserve_real_hit():
    hit = {"label": "person", "box": [0, 240, 640, 639]}
    bad = {"label": "person", "box": [642, 0, 640, 425]}
    assert center_obstacles(json.dumps({"obstacles": [hit, bad]})) == [hit]


@pytest.mark.parametrize("raw", ['oops', '{"unknown":true,"obstacles":[]}',
    '{"obstacles":[{"label":"x","box":[400,400,200,200]}]}',
    '{"obstacles":[{"label":"x","box":[0,0,1000,1000]}]}'])
def test_invalid_results_never_become_obstacles(raw):
    with pytest.raises(ValueError):
        center_obstacles(raw)
