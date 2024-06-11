.PHONY: build
build:
	mkdir -p ./build && \
	cd ./build && \
	cmake .. && \
	make

.PHONY: install
install: build
	cd ./build && \
	cmake --install .

.PHONY: run
run: build
	cd ./build/CloxCore && \
	./clox


.PHONY: clean
clean:
	rm -r ./build

.PHONY : help
help:
	@echo "The following make commands are provided:"
	@echo "... clean(delete 'build'-directory)"
	@echo "... install(build project and install)"
	@echo "... run(build project and run)"
	@echo "... build(run cmake and make)"
