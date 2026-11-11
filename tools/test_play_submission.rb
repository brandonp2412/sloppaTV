# Exercise the Fastfile with a fake Play action; never contact Google or upload files.
module PlaySubmissionHarness
  attr_reader :uploads
  attr_accessor :upload_error

  def default_platform(*)
  end

  def platform(*)
    yield
  end

  def before_all(*)
  end

  def desc(*)
  end

  def lane(*)
  end

  def upload_to_play_store(**options)
    raise upload_error if upload_error

    (@uploads ||= []) << options
  end
end

harness = Object.new.extend(PlaySubmissionHarness)
harness.instance_eval(File.read(File.expand_path('../fastlane/Fastfile', __dir__)),
                      File.expand_path('../fastlane/Fastfile', __dir__))

[true, false].each do |include_bundle|
  harness.upload_store!(track: 'production', include_bundle: include_bundle)
  options = harness.uploads.last
  raise 'Review submission must be requested explicitly' unless options[:changes_not_sent_for_review] == false
  raise 'Draft fallback must be disabled' unless options[:rescue_changes_not_sent_for_review] == false
  raise 'Wrong package' unless options[:package_name] == 'app.sloppatv'
  raise 'Wrong track' unless options[:track] == 'production'
  raise 'Wrong artifact policy' unless options[:skip_upload_aab] == !include_bundle
end

failure = RuntimeError.new('Google Play requires manual review submission')
harness.upload_error = failure
begin
  harness.upload_store!(track: 'production', include_bundle: true)
rescue RuntimeError => error
  raise unless error.equal?(failure)
else
  raise 'Submission errors must fail the lane'
end

puts 'Play submission policy passed for release and listing uploads; submission errors propagate.'
