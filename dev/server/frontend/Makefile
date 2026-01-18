

enter_node:
	docker run -it -v $(PWD)/src:/appli -w /appli node:20-alpine sh

build:
	docker build -t frontend .

run:
	docker run -it --rm -v $(PWD)/src:/appli -p 3000:3000 frontend 