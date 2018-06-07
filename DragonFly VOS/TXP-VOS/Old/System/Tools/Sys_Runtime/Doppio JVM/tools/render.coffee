#! /usr/bin/env coffee

marked   = require 'marked'
{argv}   = require('optimist').boolean 'release'
mustache = require 'mustache'
{exec}   = require 'child_process'
fs       = require 'fs'
path     = require 'path'

# Set Markdown rendering options
marked.setOptions
  smartypants: true # Enable the SmartyPants extension for nicer quotes and dashes
  smartLists:  true # Enable more intelligent list rendering

render = (opts) -> console.log(mustache.render(template, opts, partials))

fullpath = (filename) -> path.resolve __dirname, '..', 'browser', filename

readfile = (filename) -> fs.readFileSync(fullpath(filename), 'utf-8')

# The first argument passed to the script is the name of the template to be rendered
name = argv._[0]
# Read the contents of the template file
template = readfile "#{name}.mustache"

# Create an object to hold common options used by all templates
opts =
  release: argv.release

# Load the contents of partials needed by the templates into an object
partial_names = ['_navbar']
partials = {}
for p in partial_names
  partials[p] = readfile "#{p}.mustache"

# Configure any template-specific options and render the final HTML page
switch name
  when 'index'
    # Asynchronously get the current Git SHA hash
    exec 'git rev-parse HEAD', (err, stdout, stderr) ->
      throw err if err

      opts.git_hash = stdout
      opts.git_short_hash = stdout[..6]
      opts.date = (new Date()).toLocaleDateString()

      render opts

  when 'about'
    opts.about = marked readfile "_about.md"

    render opts

  when 'swing'
    render opts
