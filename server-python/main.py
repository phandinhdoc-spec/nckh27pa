import uvicorn

from thien_nhan.app import create_app
from thien_nhan.config import Config


if __name__ == "__main__":
    config = Config.load()
    uvicorn.run(create_app(config), host=config.host, port=config.port)
