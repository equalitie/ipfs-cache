
go_ipfs_cache.h go_ipfs_cache.a: go_ipfs_cache.go
	@# The first build isn't necessary, it builds a Go executable out of the Go
	@# source. It is here because the compilation is a lot faster than the one
	@# below and so in case of errors it exits a lot quicker.
	go build go_ipfs_cache.go && rm go_ipfs_cache
	go build --buildmode=c-archive go_ipfs_cache.go
