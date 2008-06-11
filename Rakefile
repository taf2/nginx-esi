# use this rake file to build, configure, and test nginx esi module
require 'rake'
require 'rake/testtask'

desc 'Setup up your build environment NGINX_SRC=/path/to/nginx/src/'
task :setup do
  nginx_src = ENV['NGINX_SRC']
  if nginx_src.nil? or !File.exist?(nginx_src)
    puts "You must set NGINX_SRC evironment variable"
    exit(1)
  end
end

desc 'Build and configure nginx, use RELEASE=true to build without debugging'
task :build do
end

desc 'Start nginx for testing'
task :start do
end

desc 'Stop nginx test server'
task :stop do
end

Rake::TestTask.new do |t|
  t.test_files = FileList["test/*_test.rb"]
  t.verbose = true
end
