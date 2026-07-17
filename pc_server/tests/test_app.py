from fastapi.testclient import TestClient

from arex_server.app import create_app
from arex_server.config import Settings


def test_health_does_not_require_openai_call():
    app = create_app(Settings(openai_api_key="", device_token="test-token"))
    with TestClient(app) as client:
        response = client.get("/health")
    assert response.status_code == 200
    assert response.json()["ok"] is True
    assert response.json()["service"] == "arex-audio-server"
    assert response.json()["connected_devices"] == []
    assert response.json()["wake_word"] == "Arex"
    assert response.json()["wake_word_enabled"] is True
    assert response.json()["openai_key_configured"] is False
    assert response.json()["cognee_enabled"] is True
