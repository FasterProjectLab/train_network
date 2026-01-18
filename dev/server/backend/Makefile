enter_dotnet:
	docker run -it --rm -v $(PWD)/src:/appli -w /appli --name websocket mcr.microsoft.com/dotnet/sdk:8.0 bash

build:
	docker build -t websocket .

run:
	docker run -it --rm -p 8080:8080 websocket

build_dev:
	docker build -t websocket-dev -f Dockerfile.dev .

run_dev:
	docker run -it --rm -p 8080:8080 -v "$(PWD)/src:/app" --name websocket-dev websocket-dev