# use this rake file to build, configure, and test nginx esi module
require 'rake'
require 'rake/testtask'
require 'yaml'

def load_config
  $config ||= YAML.load_file('.nginx_config')
end

desc 'Setup up your build environment NGINX_SRC=/path/to/nginx/src/'
task :setup do
  nginx_src = ENV['NGINX_SRC']
  if nginx_src.nil? or !File.exist?(nginx_src)
    puts "You must set NGINX_SRC enironment variable"
    exit(1)
  end
  # write configuration
  config = {
    :nginx_src => File.expand_path(nginx_src),
    :mod_src => File.expand_path(File.dirname(__FILE__))
  }
  File.open(".nginx_config","w") do|f|
    puts "Saving config: #{YAML.dump(config)}"
    f << YAML.dump(config) 
  end
end

desc 'Configure nginx'
task :configure do
  load_config
  build_string = "cd #{$config[:nginx_src]} && ./configure --prefix=#{$config[:mod_src]}/nginx/"
  if ENV['RELEASE'].nil?
    build_string << " --with-debug"
  end
  build_string << " --add-module=#{$config[:mod_src]}"
  sh build_string
end

desc 'Compile nginx'
task :compile do
  load_config
  sh "cd #{$config[:nginx_src]} && make install"
end

desc 'Build and configure nginx, use RELEASE=true to build without debugging'
task :build => [:configure,:compile]

desc 'Start nginx for testing'
task :start do
  load_config
  sh "cd nginx && ./sbin/nginx -c #{File.join(File.dirname(__FILE__),'test/config/nginx-esi.conf')}"
end

desc 'Stop nginx test server'
task :stop do
  load_config
  pid_file = "#{$config[:mod_src]}/nginx/logs/nginx.pid"
  if !File.exist?(pid_file)
    STDERR.puts "Expected nginx pid file: (#{pid_file}) not found!"
    exit(1)
  else
    pid = `cat #{pid_file}`
    sh "kill #{pid}"
    sh "rm -f #{pid_file}"
  end
end

Rake::TestTask.new do |t|
  t.test_files = FileList["test/*_test.rb"]
  t.verbose = true
end
