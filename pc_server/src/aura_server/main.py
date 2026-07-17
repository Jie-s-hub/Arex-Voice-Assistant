from __future__ import annotations

import logging

import uvicorn

from .config import Settings


def run() -> None:
    settings = Settings()
    settings.validate_runtime()
    logging.basicConfig(
        level=settings.log_level,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    uvicorn.run(
        "aura_server.app:app",
        host=settings.host,
        port=settings.port,
        log_level=settings.log_level.lower(),
    )


if __name__ == "__main__":
    run()

