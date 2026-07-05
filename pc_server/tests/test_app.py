from fastapi.testclient import TestClient

from aura_server.app import create_app
from aura_server.config import Settings


def test_health_does_not_require_openai_call():
    app = create_app(Settings(openai_api_key="", rover_token="test-token"))
    with TestClient(app) as client:
        response = client.get("/health")
    assert response.status_code == 200
    assert response.json()["ok"] is True
    assert response.json()["openai_key_configured"] is False

