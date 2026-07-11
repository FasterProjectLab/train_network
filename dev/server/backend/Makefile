enter_dotnet:
	docker run -it --rm -v $(PWD)/src:/appli -w /appli --name websocket mcr.microsoft.com/dotnet/sdk:8.0 bash

build:
	docker build -t websocket -f Dockerfile.prod .

run:
	docker run -it --rm -p 8080:8080 -p 5004:5004/udp websocket

build_dev:
	docker build -t websocket-dev -f Dockerfile.dev .

run_dev:
	docker run -it --rm -p 8080:8080 -p 5004:5004/udp -v "$(PWD)/src:/app" --name websocket-dev websocket-dev

test_flux_brut:
	ffplay -protocol_whitelist file,udp,rtp -i esp32.sdp -fflags nobuffer -flags low_delay -framedrop