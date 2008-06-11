require "#{File.dirname(__FILE__)}/help"

class EsiFilterTest < Test::Unit::TestCase

  def cached?( uri )
    $esi_dispatcher.config.cache.get( uri, {} )
  end


  def setup
    ESI::Server.start
  end

  def test_more_web_server
    results = hit([ "http://localhost:9997/esi_test_content.html", "http://localhost:9997/esi_mixed_content.html"])

		results.each do |res|
			assert_match $fragment_test1, res
			assert_no_match /<esi:/, res # assert there are no esi tags in the result
		end
    check_status results, String
	end

  def test_simple_esi
    Net::HTTP.start("localhost", 9997) do |h|
      req = h.get("/esi_test_content.html")
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
			assert_match '<html>', req.body, "Missing document head"
			assert_match $fragment_test1, req.body, "Fragment not found"
			assert_match $fragment_test2, req.body, "Fragment not found"
			assert_match '</html>', req.body, "Missing document footer"
      assert_equal %{<html>\n<head>\n</head>\n<body>\n  <h1>This is a test document</h1>\n  \n      valid\n      \n    <div>test1</div>\n\n  \n      \n    <div>test2</div>\n\n</body>\n</html>\n}, req.body
    end
  end

  def test_large_document
    Net::HTTP.start("localhost", 9997) do |h|
      req = h.get("/large-no-cache.html")
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
			assert_match $fragment_test1, req.body, "Fragment not found"
      # should have 37 fragments
      count = 0
      req.body.scan $fragment_test1 do|m|
        count += 1
      end
      assert_equal 18, count, "There should be 18 fragment_test 1's in the document"
      count = 0
      req.body.scan $fragment_test2 do|m|
        count += 1
      end
      assert_equal 19, count, "There should be 19 fragment_test 2's in the document"
    end
  end
 
  def test_reverse_proxy
    Net::HTTP.start("localhost", 9997) do |h|
      req = h.get("/content/foo.html")
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
			assert_match $fragment_test2, req.body, "Fragment not found"
    end
  end
 
  # test that we're including http headers as part of the request fragment cache
  def test_ajax_caching
    # start by making the request using the X-Requested-With Header
    Net::HTTP.start("localhost", 9997) do |h|
      req = h.get("/content/ajax_test_page.html", {"X-Requested-With" => "XMLHttpRequest"})
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
			assert_match "XMLHttpRequest", req.body, "Did not get the expected response for Javascript, should see XMLHttpRequest in the response"
    end
    # now the content should be cached so make the request again without the javascript header to get the alternate version
    Net::HTTP.start("localhost", 9997) do |h|
      req = h.get("/content/ajax_test_page.html")
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
			assert_match "HTTP Request", req.body, "Did not get the expected response for noscript"
    end
    # make the ajax request one more time for good measure
    Net::HTTP.start("localhost", 9997) do |h|
      req = h.get("/content/ajax_test_page.html", {"X-Requested-With" => "XMLHttpRequest"})
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
			assert_match "XMLHttpRequest", req.body, "Did not get the expected response for Javascript"
    end
  end

  # test that if a fragment has a <esi include in it, that response comes back with a rendered module
  def test_fragment_in_fragment
    Net::HTTP.start("localhost", 9997) do |h|
      req = h.get("/content/include_in_include.html")
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
			assert_no_match /<esi:/, req.body, "Result still contains esi includes" # assert there are no esi tags in the result
			assert_match /<div>test3<\/div>/, req.body, "assert fragment is included"
			assert_match /<div>test1<\/div>/, req.body, "assert inner fragment is included"
    end
  end

  # test whether the cache server alters the markup
  def test_markup
    Net::HTTP.start("localhost", 9997) do |h|
      req = h.get("/content/malformed_transforms.html")
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
      assert_equal File.open("#{DOCROOT}/content/malformed_transforms.html-correct").read, req.body
    end
  end

  def test_failing_fragments
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/content/500.html")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPOK, res.header.class, res.body
    end
  end

  def test_failing_fragments_can_failover
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/content/500_with_failover.html")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPOK, res.header.class, res.body
			assert_match /<div>test1<\/div>/, res.body, "The failover esi include should have been requested"
    end
  end

  def test_failing_fragments_can_failover_to_alt_attribute
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/content/500_with_failover_to_alt.html")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPOK, res.header.class, res.body
			assert_match /<div>test1<\/div>/, res.body, "The failover esi include should have been requested"
    end
  end

  def test_failing_response_from_surrogate
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/500")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal "<html><body>Internal Server Error</body></html>", res.body
      assert_equal Net::HTTPInternalServerError, res.header.class, res.body
    end
  end

  def test_static_failover
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/content/static-failover.html")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      #puts res.body
      assert_equal Net::HTTPOK, res.header.class, res.body
			assert_match /<div>test1<\/div>/, res.body, "Normal esi include tags failed"
			assert_match /<div>test1<\/div>/, res.body, "Normal esi include tags failed"
			assert_match /<a href="www.akamai.com">www.example.com <\/a>/, res.body, "Static Failover failed"
    end
  end

  def test_cookies_proxied
    Net::HTTP.start("localhost", 9997) do |h|
      post_str = "key=value"
      req = h.post("/post_test",post_str)
      assert_not_nil req
      assert_not_nil req.header
      assert_not_nil req.body
      assert_equal Net::HTTPOK, req.header.class
      assert_match TEST_COOKIE1, req.header["Set-Cookie"]
      assert_match TEST_COOKIE2, req.header["Set-Cookie"]
      assert_match TEST_COOKIE3, req.header["Set-Cookie"]
    end
  end

	def test_markup_in_attempt_passed_through
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/content/test3.html")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPOK, res.header.class
			assert_match /<p>Some markup that should not be lost<\/p>/, res.body
		end
	end

=begin
  def test_surrogate_control_header
    # first tell the esi handler to use the surrogate control header
    $esi_dispatcher.config.config.send(:store,:enable_for_surrogate_only, true)
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/404")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPNotFound, res.header.class
			assert_match /#{$fragment_test1}/, res.body
		end
    $esi_dispatcher.config.config.send(:store,:enable_for_surrogate_only, nil)
 
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/404-no-surrogate")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPNotFound, res.header.class
			assert_no_match /#{$fragment_test1}/, res.body
		end
  end
=end

  def test_invalidation
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/esi_test_content.html")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPOK, res.header.class
			assert_match $fragment_test1, res.body, "Fragment not found"
			assert_match $fragment_test2, res.body, "Fragment not found"
    end

    assert cached?( FRAGMENT_TEST1_URI ), "Error Fragment should be cached!"
    assert cached?( FRAGMENT_TEST2_URI ), "Error Fragment should be cached!"
 
		Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/esi_invalidate.html")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPOK, res.header.class
    end
	
    assert !cached?( FRAGMENT_TEST1_URI ), "Error Fragment 1 should not be cached!"
    assert !cached?( FRAGMENT_TEST2_URI ), "Error Fragment 2 should not be cached!"
  end

  def test_max_age_include_attribute_cache_ttl
    Net::HTTP.start("localhost", 9997) do |h|
      res = h.get("/esi_max_age_varies.html")
      assert_not_nil res
      assert_not_nil res.header
      assert_not_nil res.body
      assert_equal Net::HTTPOK, res.header.class
			assert_match $fragment_test1, res.body, "Fragment not found"
			assert_match $fragment_test2, res.body, "Fragment not found"
    end

    assert cached?( FRAGMENT_TEST1_URI ), "Error Fragment should be cached!"
    assert !cached?( FRAGMENT_TEST2_URI ), "Error Fragment should not be cached!"
  end


end

